/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is ChatZilla.
 *
 * The Initial Developer of the Original Code is James Ross.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Ross <silver@warwickcompsoc.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function initNetworks()
{
    var networks = new Object();

    // Set up default network list.
    networks["moznet"] = {
        displayName:  "moznet",
        isupportsKey: "Mozilla",
        servers: [{hostname: "irc.mozilla.org", port:6667},
                  {hostname: "irc.mozilla.org", port:6697, isSecure: true}]};
    networks["hybridnet"] = {
        displayName:  "hybridnet",
        isupportsKey: "",
        servers: [{hostname: "irc.ssc.net", port: 6667}]};
    networks["slashnet"] = {
        displayName:  "slashnet",
        isupportsKey: "",
        servers: [{hostname: "irc.slashnet.org", port:6667}]};
    networks["dalnet"] = {
        displayName:  "dalnet",
        isupportsKey: "",
        servers: [{hostname: "irc.dal.net", port:6667}]};
    networks["undernet"] = {
        displayName:  "undernet",
        isupportsKey: "",
        servers: [{hostname: "irc.undernet.org", port:6667}]};
    networks["webbnet"] = {
        displayName:  "webbnet",
        isupportsKey: "",
        servers: [{hostname: "irc.webbnet.info", port:6667}]};
    networks["quakenet"] = {
        displayName:  "quakenet",
        isupportsKey: "",
        servers: [{hostname: "irc.quakenet.org", port:6667}]};
    networks["ircnet"] = {
        displayName:  "ircnet",
        isupportsKey: "",
        servers: [{hostname: "irc.ircnet.com", port:6667}]};
    networks["freenode"] = {
        displayName:  "freenode",
        isupportsKey: "",
        servers: [{hostname: "irc.freenode.net", port:6667}]};
    networks["serenia"] = {
        displayName:  "serenia",
        isupportsKey: "",
        servers: [{hostname: "chat.serenia.net", port:9999, isSecure: true}]};
    networks["efnet"] = {
        displayName:  "efnet",
        isupportsKey: "",
        servers: [{hostname: "irc.prison.net", port: 6667},
                  {hostname: "irc.magic.ca", port: 6667}]};
    networks["hispano"] = {
        displayName:  "hispano",
        isupportsKey: "",
        servers: [{hostname: "irc.irc-hispano.org", port: 6667}]};

    for (var name in networks)
        networks[name].name = name;

    var builtInNames = keys(networks);

    var userNetworkList = new Array();

    // Load the user's network list.
    var networksFile = new nsLocalFile(client.prefs["profilePath"]);
    networksFile.append("networks.txt");
    if (networksFile.exists())
    {
        var networksLoader = new TextSerializer(networksFile);
        if (networksLoader.open("<"))
        {
            var item = networksLoader.deserialize();
            if (isinstance(item, Array))
                userNetworkList = item;
            else
                dd("Malformed networks file!");
            networksLoader.close();
        }
    }

    // Merge the user's network list with the default ones.
    for (var i = 0; i < userNetworkList.length; i++)
        networks[userNetworkList[i].name] = userNetworkList[i];

    /* Flag up all networks that are built-in, so they can be handled properly.
     * We need to do this last so that it ensures networks overridden by the
     * user's networks.txt are still flagged properly.
     */
    for (var i = 0; i < builtInNames.length; i++)
        networks[builtInNames[i]].isBuiltIn = true;

    // Push network list over to client.networkList.
    client.networkList = new Array();
    for (var name in networks)
        client.networkList.push(networks[name]);

    // Sync to client.networks.
    networksSyncFromList();
}

function networksSyncToList()
{
    // Stores indexes of networks that should be kept.
    var networkMap = new Object();

    // Copy to and update client.networkList from client.networks.
    for (var name in client.networks)
    {
        var net = client.networks[name];
        /* Skip temporary networks, as they're created to wrap standalone
         * servers only.
         */
        if (net.temporary)
            continue;

        // Find the network in the networkList, if it exists.
        var listNet = null;
        for (var i = 0; i < client.networkList.length; i++)
        {
            if (client.networkList[i].name == name)
            {
                listNet = client.networkList[i];
                networkMap[i] = true;
                break;
            }
        }

        // Network not in list, so construct a shiny new one.
        if (listNet == null)
        {
            var listNet = { name: name, displayName: name, isupportsKey: "" };

            // Collect the RPL_ISUPPORTS "network name" if available.
            if (("primServ" in net) && net.primServ &&
                ("supports" in net.primServ) && net.primServ.supports &&
                ("network" in net.primServ.supports))
            {
                listNet.isupportsKey = net.primServ.supports["network"];
            }

            client.networkList.push(listNet);
            networkMap[client.networkList.length - 1] = true;
        }

        // Populate server list (no merging here).
        listNet.servers = new Array();
        for (i = 0; i < net.serverList.length; i++)
        {
            var serv = net.serverList[i];

            // Find this server in the list...
            var listServ = null;
            for (var j = 0; j < listNet.servers.length; j++)
            {
                if ((serv.hostname == listNet.servers[j].hostname) &&
                    (serv.port     == listNet.servers[j].port))
                {
                    listServ = listNet.servers[j];
                    break;
                }
            }

            // ...and add a new one if it isn't found.
            if (listServ == null)
            {
                listServ = { hostname: serv.hostname, port: serv.port,
                             isSecure: serv.isSecure, password: null };
                listNet.servers.push(listServ);
            }

            listServ.isSecure = serv.isSecure;
            // Update the saved password (!! FIXME: plaintext password !!).
            listServ.password = serv.password;
        }
    }

    // Remove any no-longer existing networks.
    var index = 0;    // (current pointer into client.networkList)
    var mapIndex = 0; // (original position pointer)
    while (index < client.networkList.length)
    {
        if (mapIndex in networkMap)
        {
            index++;
            mapIndex++;
            continue;
        }

        var listNet = client.networkList[index];
        // Not seen this network in the client.networks collection.
        if (("isBuiltIn" in listNet) && listNet.isBuiltIn)
        {
            // Network is a built-in. Replace with dummy.
            client.networkList[index] = { name: listNet.name, isDeleted: true };
            index++;
        }
        else
        {
            // Network is not a built-in, just nuke.
            // Note: don't do index++ here because we've removed the item.
            client.networkList.splice(index, 1);
        }
        mapIndex++;
    }
}

function networksSyncFromList()
{
    var networkMap = new Object();

    // Copy to and update client.networks from client.networkList.
    for (var i = 0; i < client.networkList.length; i++)
    {
        var listNet = client.networkList[i];
        networkMap[listNet.name] = true;

        if ("isDeleted" in listNet)
        {
            /* This is a dummy entry that indicates a removed built-in network.
             * Remove the network from client.networks if it exists, and then
             * skip onto the next...
             */
            if (listNet.name in client.networks)
                client.removeNetwork(listNet.name);
            continue;
        }

        // Create new network object if necessary.
        var net = null;
        if (!(listNet.name in client.networks))
            client.addNetwork(listNet.name, []);

        // Get network object and make sure server list is empty.
        net = client.networks[listNet.name];
        net.clearServerList();

        // Make sure real network knows if it is a built-in one.
        if ("isBuiltIn" in listNet)
            net.isBuiltIn = listNet.isBuiltIn;

        // Update server list.
        for (var j = 0; j < listNet.servers.length; j++)
        {
            var listServ = listNet.servers[j];

            // Make sure these exist.
            if (!("isSecure" in listServ))
                listServ.isSecure = false;
            if (!("password" in listServ))
                listServ.password = null;

            // NOTE: this must match the name given by CIRCServer.
            var servName = listServ.hostname + ":" + listServ.port;

            var serv = null;
            if (!(servName in net.servers))
            {
                net.addServer(listServ.hostname, listServ.port,
                              listServ.isSecure, listServ.password);
            }
            serv = net.servers[servName];

            serv.isSecure = listServ.isSecure;
            serv.password = listServ.password;
        }
    }

    // Remove network objects that aren't in networkList.
    for (var name in client.networks)
    {
        // Skip temporary networks, as they don't matter.
        if (client.networks[name].temporary)
            continue;
        if (!(name in networkMap))
            client.removeNetwork(name);
    }
}

function networksSaveList()
{
    try
    {
        var networksFile = new nsLocalFile(client.prefs["profilePath"]);
        networksFile.append("networks.txt");
        var networksLoader = new TextSerializer(networksFile);
        if (networksLoader.open(">"))
        {
            networksLoader.serialize(client.networkList);
            networksLoader.close();
        }
    }
    catch(ex)
    {
        display("ERROR: " + formatException(ex), MT_ERROR);
    }
}
