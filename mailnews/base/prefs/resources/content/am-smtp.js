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
 *   Scott MacGregor <mscott@mozilla.org>
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

const nsIPromptService = Components.interfaces.nsIPromptService;
var smtpService = Components.classes["@mozilla.org/messengercompose/smtp;1"].getService(Components.interfaces.nsISmtpService);

var gSmtpServerListWindow = 
{
  mBundle: null,
  mServerList: null,
  mAddButton: null,
  mEditButton: null,
  mDeleteButton: null,
  mSetDefaultServerButton: null,

  onLoad: function()
  {
    parent.onPanelLoaded('am-smtp.xul');

    this.mBundle = document.getElementById("bundle_messenger");
    this.mServerList = document.getElementById("smtpList");
    this.mAddButton = document.getElementById("addButton");
    this.mEditButton = document.getElementById("editButton");
    this.mDeleteButton = document.getElementById("deleteButton");
    this.mSetDefaultServerButton = document.getElementById("setDefaultButton");

    this.refreshServerList("", false);
  },

  onSelectionChanged: function(aEvent)
  {
    if (this.mServerList.selectedItems.length <= 0) 
      return;

    var server = this.getSelectedServer();
    this.updateButtons(server);
    this.updateServerInfoBox(server);    
  },

  onDeleteServer: function (aEvent)
  {
    var server = this.getSelectedServer();
    if (server)
    {
      // confirm deletion
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPromptService);
      var cancel = promptService.confirmEx(window, this.mBundle.getString('smtpServers-confirmServerDeletionTitle'), 
                                           this.mBundle.getFormattedString('smtpServers-confirmServerDeletion', [server.hostname], 1),
                                           (nsIPromptService.BUTTON_TITLE_YES * nsIPromptService.BUTTON_POS_0) + 
                                           (nsIPromptService.BUTTON_TITLE_NO * nsIPromptService.BUTTON_POS_1),
                                           null, null, null, null, { });

      if (!cancel)
      {
        smtpService.deleteSmtpServer(server);
        parent.replaceWithDefaultSmtpServer(server.key);
        this.refreshServerList("", true);
      }
    } 
  },

  onAddServer: function (aEvent)
  {
    this.openServerEditor(null);
  },

  onEditServer: function (aEvent)
  {
    if (this.mServerList.selectedItems.length <= 0) 
      return;
    this.openServerEditor(this.getSelectedServer());
  },

  onSetDefaultServer: function(aEvent)
  {
    if (this.mServerList.selectedItems.length <= 0) 
      return;

    smtpService.defaultServer = this.getSelectedServer();
    this.refreshServerList(smtpService.defaultServer.key, true);
  },

  updateButtons: function(aServer)
  {
    // can't delete default server
    if (smtpService.defaultServer == aServer) 
    {
      this.mSetDefaultServerButton.setAttribute("disabled", "true");
      this.mDeleteButton.setAttribute("disabled", "true");
    }
    else 
    {
      this.mSetDefaultServerButton.removeAttribute("disabled");
      this.mDeleteButton.removeAttribute("disabled");
    }
  },

  updateServerInfoBox: function(aServer)
  {
    var noneSelected = this.mBundle.getString("smtpServerList-NotSpecified");

    document.getElementById('nameValue').value = aServer.hostname;
    document.getElementById('descriptionValue').value = aServer.description || noneSelected;
    document.getElementById('portValue').value = aServer.port;
    document.getElementById('userNameValue').value = aServer.username || noneSelected;
    document.getElementById('useSecureConnectionValue').value = this.mBundle.getString("smtpServer-SecureConnection-Type-" + 
                                                                aServer.trySSL);
  },

  refreshServerList: function(aServerKeyToSelect, aFocusList)
  {
    // remove all children
    while (this.mServerList.hasChildNodes())
      this.mServerList.removeChild(this.mServerList.lastChild);

    var defaultServer = smtpService.defaultServer;
    this.fillSmtpServers(this.mServerList, smtpService.smtpServers, defaultServer);

    if (aServerKeyToSelect)
      this.mServerList.selectItem(this.mServerList.getElementsByAttribute("key", aServerKeyToSelect)[0]);
    else // select the default server
      this.mServerList.selectItem(this.mServerList.getElementsByAttribute("default", "true")[0]);
    
    if (aFocusList)
      this.mServerList.focus();
  },

  fillSmtpServers: function(aListBox, aServers, aDefaultServer)
  {
    if (!aListBox || !aServers) 
      return;

    var serverCount = aServers.Count();
    for (var i=0; i < serverCount; i++) 
    {
      var server = aServers.QueryElementAt(i, Components.interfaces.nsISmtpServer);
      var isDefault = (aDefaultServer.key == server.key);
      //ToDoList: add code that allows for the redirector type to specify whether to show values or not
      if (!server.redirectorType) 
      {
        var listitem = this.createSmtpListItem(server, isDefault);
        aListBox.appendChild(listitem);
      }
    }    
  },

  createSmtpListItem: function(aServer, aIsDefault)
  {
    var listitem = document.createElement("listitem");
    var serverName = "";

    if (aServer.description)
      serverName = aServer.description + ' - ';
    else if (aServer.username)
      serverName = aServer.username + ' - ';
    
    serverName += aServer.hostname;
      
    if (aIsDefault)
    {
      serverName += " " + this.mBundle.getString("defaultServerTag");
      listitem.setAttribute("default", "true");
    }

    listitem.setAttribute("label", serverName);
    listitem.setAttribute("key", aServer.key);
    listitem.setAttribute("class", "smtpServerListItem");
    
    // give it some unique id
    listitem.id = "smtpServer." + aServer.key;
    return listitem;
  },

  openServerEditor: function(aServer)
  {
    var args = {server: aServer,
                result: false,
                addSmtpServer: ""};

    window.openDialog("chrome://messenger/content/SmtpServerEdit.xul",
                      "smtpEdit", "chrome,titlebar,modal,centerscreen", args);
    
    // now re-select the server which was just added
    if (args.result)          
      this.refreshServerList(aServer ? aServer.key : args.addSmtpServer, true);
  
    return args.result;
  },

  getSelectedServer: function()
  {
    var serverKey = this.mServerList.selectedItems[0].getAttribute("key");
    return smtpService.getServerByKey(serverKey); 
  }
};


