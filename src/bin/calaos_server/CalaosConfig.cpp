/******************************************************************************
 **  Copyright (c) 2006-2017, Calaos. All Rights Reserved.
 **
 **  This file is part of Calaos.
 **
 **  Calaos is free software; you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation; either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Calaos is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Foobar; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **
 ******************************************************************************/
#include "CalaosConfig.h"

using namespace Calaos;

Config::Config()
{
    loadStateCache();

    saveCacheTimer = std::make_shared<Timer>(60.0, [this]() { saveStateCache(); });
}

Config::~Config()
{
}

void Config::LoadConfigIO()
{
    std::string file = Utils::getConfigFile(IO_CONFIG);

    if (!FileUtils::exists(file))
    {
        std::ofstream conf(file.c_str(), std::ofstream::out);
        conf << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" << std::endl;
        conf << "<calaos:ioconfig xmlns:calaos=\"http://www.calaos.fr\">" << std::endl;
        conf << "<calaos:home></calaos:home>" << std::endl;
        conf << "</calaos:ioconfig>" << std::endl;
        conf.close();
    }

    TiXmlDocument document(file);

    if (!document.LoadFile())
    {
        cError() <<  "There was a parse error";
        cError() <<  document.ErrorDesc();
        cError() <<  "In file " << file << " At line " << document.ErrorRow();

        exit(-1);
    }

    TiXmlHandle docHandle(&document);

    TiXmlElement *room_node = docHandle.FirstChildElement("calaos:ioconfig").FirstChildElement("calaos:home").FirstChildElement().ToElement();
    for(; room_node; room_node = room_node->NextSiblingElement())
    {
        if (room_node->ValueStr() == "calaos:room" &&
            room_node->Attribute("name") &&
            room_node->Attribute("type"))
        {
            string name, type;
            int hits = 0;

            name = room_node->Attribute("name");
            type = room_node->Attribute("type");
            if (room_node->Attribute("hits"))
                room_node->Attribute("hits", &hits);

            Room *room = new Room(name, type, hits);
            ListeRoom::Instance().Add(room);

            room->LoadFromXml(room_node);
        }
    }

    cInfo() <<  "Done. ";
}

void Config::SaveConfigIO()
{
    string file = Utils::getConfigFile(IO_CONFIG);
    string tmp = file + "_tmp";

    cInfo() <<  "Saving " << file << "...";

    TiXmlDocument document;
    TiXmlDeclaration *decl = new TiXmlDeclaration("1.0", "UTF-8", "");
    TiXmlElement *ionode = new TiXmlElement("calaos:ioconfig");
    ionode->SetAttribute("xmlns:calaos", "http://www.calaos.fr");
    document.LinkEndChild(decl);
    document.LinkEndChild(ionode);
    TiXmlElement *node = new TiXmlElement("calaos:home");
    ionode->LinkEndChild(node);

    for (int i = 0;i < ListeRoom::Instance().size();i++)
    {
        Room *room = ListeRoom::Instance().get_room(i);
        room->SaveToXml(node);
    }

    if (document.SaveFile(tmp))
    {
        unlink(file.c_str());
        rename(tmp.c_str(), file.c_str());
    }

    cInfo() <<  "Done.";
}

void Config::LoadConfigRule()
{
    std::string file = Utils::getConfigFile(RULES_CONFIG);

    if (!FileUtils::exists(file))
    {
        std::ofstream conf(file.c_str(), std::ofstream::out);
        conf << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" << std::endl;
        conf << "<calaos:rules xmlns:calaos=\"http://www.calaos.fr\">" << std::endl;
        conf << "</calaos:rules>" << std::endl;
        conf.close();
    }

    TiXmlDocument document(file);

    if (!document.LoadFile())
    {
        cError() <<  "There was a parse error in " << file;
        cError() <<  document.ErrorDesc();
        cError() <<  "In file " << file << " At line " << document.ErrorRow();

        exit(-1);
    }

    TiXmlHandle docHandle(&document);

    TiXmlElement *rule_node = docHandle.FirstChildElement("calaos:rules").FirstChildElement().ToElement();

    if (!rule_node)
    {
        cError() <<  "Error, <calaos:rules> node not found in file " << file;
    }

    for(; rule_node; rule_node = rule_node->NextSiblingElement())
    {
        if (rule_node->ValueStr() == "calaos:rule" &&
            rule_node->Attribute("name") &&
            rule_node->Attribute("type"))
        {
            string name, type;

            name = rule_node->Attribute("name");
            type = rule_node->Attribute("type");

            Rule *rule = new Rule(type, name);
            rule->LoadFromXml(rule_node);

            ListeRule::Instance().Add(rule);
        }
    }

    cInfo() <<  "Done. " << ListeRule::Instance().size() << " rules loaded.";
}

void Config::SaveConfigRule()
{
    string file = Utils::getConfigFile(RULES_CONFIG);
    string tmp = file + "_tmp";

    cInfo() <<  "Saving " << file << "...";

    TiXmlDocument document;
    TiXmlDeclaration *decl = new TiXmlDeclaration("1.0", "UTF-8", "");
    TiXmlElement *rulesnode = new TiXmlElement("calaos:rules");
    rulesnode->SetAttribute("xmlns:calaos", "http://www.calaos.fr");
    document.LinkEndChild(decl);
    document.LinkEndChild(rulesnode);

    for (int i = 0;i < ListeRule::Instance().size();i++)
    {
        Rule *rule = ListeRule::Instance().get_rule(i);
        rule->SaveToXml(rulesnode);
    }

    if (document.SaveFile(tmp))
    {
        unlink(file.c_str());
        rename(tmp.c_str(), file.c_str());
    }

    cInfo() <<  "Done.";
}

void Config::loadStateCache()
{
    string file = Utils::getCacheFile("iostates.cache");
    cache_states.clear();

    std::ifstream cacheStream;
    cacheStream.open(file);
    if (!cacheStream.is_open())
    {
        cWarning() <<  "Could not open iostates.cache for read !";
        return;
    }

    Json jcache;
    try
    {
        jcache = Json::parse(cacheStream);
        if (!jcache.is_object())
            throw (invalid_argument(string("Json cache is not an object")));
    }
    catch (const std::exception &e)
    {
        cWarning() << "Error parsing " << file << ":" << e.what();
        return;
    }

    for (Json::iterator it = jcache.begin(); it != jcache.end(); ++it)
    {
        cache_states[it.key()] = it.value();
    }

    cInfo() <<  "States cache read successfully.";
}

void Config::saveStateCache()
{
    string file = Utils::getCacheFile("iostates.cache");
    string tmp = file + ".tmp";

    Json jcache(cache_states);
    std::ofstream fout;
    fout.open(tmp, std::ofstream::out | std::ofstream::trunc);
    if (!fout.is_open())
    {
        cWarning() <<  "Could not open iostates.cache for write !";
        return;
    }

    fout << jcache.dump(4);
    fout.close();

    FileUtils::unlink(file);
    FileUtils::rename(tmp, file);

    cInfo() <<  "State cache file written successfully (" << file << ")";
}

void Config::SaveValueIO(string id, string value, bool save)
{
    cache_states[id] = value;
    if (save)
        saveStateCache();
}

bool Config::ReadValueIO(string id, string &value)
{
    if (cache_states.find(id) != cache_states.end())
    {
        value = cache_states[id];
        return true;
    }

    return false;
}
