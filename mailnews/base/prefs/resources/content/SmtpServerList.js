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
var serverList;                 // the root <tree> node

var addButton;
var editButton;
var deleteButton;
var setDefaultButton;
var messengerStrings;

var hasEdited=false;            // whether any kind of edits have occured

// event handlersn
function onLoad()
{
    if (!messengerStrings)
        messengerStrings = srGetStrBundle("chrome://messenger/locale/messenger.properties");    
    if (!smtpService)
        smtpService = Components.classes["@mozilla.org/messengercompose/smtp;1"].getService(Components.interfaces.nsISmtpService);


    serverList = document.getElementById("smtpTree");
    addButton        = document.getElementById("addButton");
    editButton       = document.getElementById("editButton");
    deleteButton     = document.getElementById("deleteButton");
    setDefaultButton = document.getElementById("setDefaultButton");
    
    refreshServerList();

    doSetOKCancel(onOk, 0);
}

function onSelectionChange(event)
{
    updateButtons();
}

function onDelete(event)
{
    var server = getSelectedServer();
    if (!server) return;
    smtpService.deleteSmtpServer(server);
    refreshServerList();
}

function onAdd(event)
{
    openServerEditor(null);
}

function onEdit(event)
{
    if (serverList.selectedItems.length <= 0) return;

    var server = getSelectedServer();
    openServerEditor(server);
}

function onSetDefault(event)
{
    if (serverList.selectedItems.length <= 0) return;

    smtpService.defaultServer = getSelectedServer();
    refreshServerList();
}

function onOk()
{
    window.arguments[0].result = true;
    window.close();
}

function updateButtons()
{
    if (serverList.selectedItems.length <= 0) {
        editButton.setAttribute("disabled", "true");
        deleteButton.setAttribute("disabled", "true");
        setDefaultButton.setAttribute("disabled", "true");
    } else {
        editButton.removeAttribute("disabled");
        
        // can't delete default server
        var server = getSelectedServer();
        if (smtpService.defaultServer == server) {
            dump("Selected default server!\n");
            setDefaultButton.setAttribute("disabled", "true");
            deleteButton.setAttribute("disabled", "true");
        }
        else {
            setDefaultButton.removeAttribute("disabled");
            deleteButton.removeAttribute("disabled");
        }
    }

}

function refreshServerList()
{
    // save selection
    var oldSelectedIds = new Array;
    serverList.clearItemSelection();
    var selectedItems = serverList.selectedItems;
    for (var i=0; i< selectedItems.length; i++)
        oldSelectedIds[i] = selectedItems[0].id;

    var treeChildren = document.getElementById("smtpTreeChildren");

    // remove all children
    while (treeChildren.hasChildNodes())
        treeChildren.removeChild(treeChildren.lastChild);

    var defaultServer = smtpService.defaultServer;
    fillSmtpServers(treeChildren,smtpService.smtpServers, defaultServer);

    // restore selection
    for (var i=0; i< oldSelectedIds.length; i++) {
        var element = document.getElementById(oldSelectedIds[i]);
        if (element)
            serverList.selectItem(element);
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
    // give it some unique id
    treeitem.id = "smtpServer." + server.key;
    treerow.appendChild(treecell);
    treeitem.appendChild(treerow);

    return treeitem;
}

function openServerEditor(serverarg)
{
    var args = {server: serverarg,
                result: false};
    window.openDialog("chrome://messenger/content/SmtpServerEdit.xul",
                      "smtpEdit", "chrome,titlebar,modal", args);
    if (args.result) {
        refreshServerList();
        hasEdited = true;
    }
    return args.result;
}

function getSelectedServer()
{
    var serverKey = serverList.selectedItems[0].getAttribute("key");
    var server = smtpService.getServerByKey(serverKey);
    
    return server;
}
