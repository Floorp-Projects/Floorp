/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var smtpService;
var serverList;                 // the root <listbox> node

var addButton;
var editButton;
var deleteButton;
var setDefaultButton;
var gMessengerBundle;

var hasEdited=false;            // whether any kind of edits have occured

var gOldDefaultSmtpServer;
var gAddedSmtpServers = new Array;
var gDeletedSmtpServers = new Array;

// event handlersn
function onLoad()
{
    gMessengerBundle = document.getElementById("bundle_messenger");
    if (!smtpService)
        smtpService = Components.classes["@mozilla.org/messengercompose/smtp;1"].getService(Components.interfaces.nsISmtpService);


    serverList = document.getElementById("smtpList");
    addButton        = document.getElementById("addButton");
    editButton       = document.getElementById("editButton");
    deleteButton     = document.getElementById("deleteButton");
    setDefaultButton = document.getElementById("setDefaultButton");

    gOldDefaultSmtpServer = smtpService.defaultServer;
    refreshServerList();
}

function onSelectionChange(event)
{
    updateButtons();
}

function onDelete(event)
{
    var server = getSelectedServer();
    if (!server) return;

//  Instead of deleting the server we store the key in the array and delete it later only if the 
//  window is not cancelled.

    gDeletedSmtpServers[gDeletedSmtpServers.length] = server.key;
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
    window.opener.onExitAdvancedDialog(gDeletedSmtpServers,true);
    return true;
}

function onCancel()
{
  window.arguments[0].result = false;

  // there might not be an old one.  see bug #176056
  if (gOldDefaultSmtpServer)
    smtpService.defaultServer = gOldDefaultSmtpServer;

  window.opener.onExitAdvancedDialog(gAddedSmtpServers,false);
  return true;
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
    serverList.clearSelection();
    var selectedItems = serverList.selectedItems;
    for (var i=0; i< selectedItems.length; i++)
        oldSelectedIds[i] = selectedItems[0].id;

    // remove all children
    while (serverList.hasChildNodes())
        serverList.removeChild(serverList.lastChild);

    var defaultServer = smtpService.defaultServer;
    fillSmtpServers(serverList, smtpService.smtpServers, defaultServer);
    // restore selection
    for (i = 0; i < oldSelectedIds.length; ++i) {
        var element = document.getElementById(oldSelectedIds[i]);
        if (element)
            serverList.selectItem(element);
    }
}

// helper functions

function fillSmtpServers(listbox, servers, defaultServer)
{
    if (!listbox) return;
    if (!servers) return;

    var serverCount = servers.Count();
    for (var i=0 ; i<serverCount; i++) {
        var server = servers.QueryElementAt(i, Components.interfaces.nsISmtpServer);
        var isDefault = (defaultServer.key == server.key);
        //ToDoList: add code that allows for the redirector type to specify whether to show values or not
        if (!server.redirectorType && !deletedServer(server.key)) 
        {
          var listitem = createSmtpListItem(server, isDefault);
          listbox.appendChild(listitem);
        }
    }

}

function createSmtpListItem(server, isDefault)
{
    var listitem = document.createElement("listitem");

    var hostname = server.hostname;
    if (server.port) {
      hostname = hostname + ":" + server.port;
    }
    
    if (isDefault)
        hostname += " " + gMessengerBundle.getString("defaultServerTag");

    listitem.setAttribute("label", hostname);
    listitem.setAttribute("key", server.key);
    // give it some unique id
    listitem.id = "smtpServer." + server.key;

    return listitem;
}

function deletedServer(serverkey)
{
  for (var index in gDeletedSmtpServers) {
    if (serverkey == gDeletedSmtpServers[index])
      return true;
  }
  return false;
}

function openServerEditor(serverarg)
{
    var args = {server: serverarg,
                result: false,
                addSmtpServer: ""};
    window.openDialog("chrome://messenger/content/SmtpServerEdit.xul",
                      "smtpEdit", "chrome,titlebar,modal", args);
    if (args.result) {
        if (args.addSmtpServer)
          gAddedSmtpServers[gAddedSmtpServers.length] = args.addSmtpServer;
          
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
