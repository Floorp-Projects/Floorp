/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

var smtpService;
var serverList;

var stringBundle;

function onLoad()
{
    var selectedServer = null;
    if (window.arguments && window.arguments[0] && window.arguments[0].server)
        selectedServer = window.arguments[0].server;

    dump("pre-select server: " + selectedServer + "\n");
    
    if (!smtpService)
        smtpService = Components.classes["component://netscape/messengercompose/smtp"].getService(Components.interfaces.nsISmtpService);

    if (!stringBundle)
        stringBundle = srGetStrBundle("chrome://messenger/locale/messenger.properties");

    //serverList = document.getElementById("smtpPopup");

    refreshServerList(smtpService.smtpServers, selectedServer);
    
    doSetOKCancel(onOk, 0);
}

function onOk()
{
    dump("serverList.selectedItem = " + document.getElementById("smtpPopup").getAttribute("selectedKey") + "\n");
    window.close();
}

function refreshServerList(servers, selectedServer)
{
    var defaultMenuItem = document.createElement("menuitem");
    defaultMenuItem.setAttribute("value", stringBundle.GetStringFromName("useDefaultServer"));
    document.getElementById("smtpPopup").appendChild(defaultMenuItem);
        
    var serverCount = servers.Count();

    for (var i=0; i<serverCount; i++) {
        var server = servers.QueryElementAt(i, Components.interfaces.nsISmtpServer);
        var menuitem = document.createElement("menuitem");
        menuitem.setAttribute("value", server.hostname);
        menuitem.setAttribute("id", server.serverURI);
        menuitem.setAttribute("key", server.key);
        
        document.getElementById("smtpPopup").appendChild(menuitem);
        if (server == selectedServer) {
            dump("found the selected one!\n");
            serverList.setAttribute("selectedKey", server.key);
        }
    }

}

function onSelected(event)
{
    serverList.setAttribute("selectedKey", event.target.getAttribute("key"));
    dump("Something selected on " + event.target.tagName + "\n");
}
