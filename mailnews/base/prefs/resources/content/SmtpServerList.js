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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

var smtpService;
var serverList;

var addButton;
var editButton;
var deleteButton;
var setDefaultButton;
var messengerStrings;

// event handlersn
function onLoad()
{
    dump("loading smtp dialog..\n");

    if (!messengerStrings)
        messengerStrings = srGetStrBundle("chrome://messenger/locale/messenger.properties");    
    if (!smtpService)
        smtpService = Components.classes["component://netscape/messengercompose/smtp"].getService(Components.interfaces.nsISmtpService);

    var defaultServer = smtpService.defaultServer;
    fillSmtpServers(document.getElementById("smtpTreeChildren"),
                    smtpService.smtpServers, defaultServer);

    serverList = document.getElementById("smtpTree");
    addButton        = document.getElementById("addButton");
    editButton       = document.getElementById("editButton");
    deleteButton     = document.getElementById("deleteButton");
    setDefaultButton = document.getElementById("setDefaultButton");
}

function onSelectionChange(event)
{
    updateButtons();
}

function onDelete(event)
{
    if (serverList.selectedItems.length <= 0) return;



}


function onAdd(event)
{
    window.openDialog("chrome://messenger/content/SmtpServerEdit.xul",
                      "smtpEdit", "chrome,modal");
}

function onEdit(event)
{
    if (serverList.selectedItems.length <= 0) return;

    var serverKey = serverList.selectedItems[0].getAttribute("key");
    var serverArg = smtpService.getServerByKey(serverKey);
    
    window.openDialog("chrome://messenger/content/SmtpServerEdit.xul",
                      "smtpEdit", "chrome,modal", {server: serverArg} );
}

function onSetDefault(event)
{
    if (serverList.selectedItems.length <= 0) return;

}

function onOk()
{


}

function updateButtons()
{
    if (serverList.selectedItems.length <= 0) {
        editButton.setAttribute("disabled", "true");
        deleteButton.setAttribute("disabled", "true");
        setDefaultButton.setAttribute("disabled", "true");
    } else {
        editButton.removeAttribute("disabled");
        deleteButton.removeAttribute("disabled");
        setDefaultButton.removeAttribute("disabled");
    }

}

// helper functions

function fillSmtpServers(treechildren, servers, defaultServer)
{
    if (!treechildren) return;
    if (!servers) return;

    var serverCount = servers.Count();
    for (var i=0 ; i<serverCount; i++) {
        var server = servers.QueryElementAt(i, Components.interfaces.nsISmtpServer);
        var isDefault = (defaultServer.key == server.key);
        var treeitem = createSmtpTreeItem(server, isDefault);
        treechildren.appendChild(treeitem);
        dump("server[" + i + "] = " + server.hostname + "\n");
    }

}

function createSmtpTreeItem(server, isDefault)
{
    var treeitem = document.createElement("treeitem");
    var treerow = document.createElement("treerow");
    var treecell = document.createElement("treecell");

    var hostname = server.hostname;
    if (isDefault)
        hostname += " " + messengerStrings.GetStringFromName("defaultServerTag");
    
    treecell.setAttribute("value", hostname);
    treeitem.setAttribute("key", server.key);
    treerow.appendChild(treecell);
    treeitem.appendChild(treerow);

    return treeitem;
}

