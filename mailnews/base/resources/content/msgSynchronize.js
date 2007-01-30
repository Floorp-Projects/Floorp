/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   dianesun@netscape.com
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

var gSyncMail = false;
var gSyncNews = false;
var gSendMessage = true;
var gWorkOffline = false;
var gSynchronizeTree = null;
var gAccountManager = null;
var gParentMsgWindow;
var gMsgWindow;

var gInitialFolderStates = {};

// RDF needs to be defined for GetFolderAttribute in msgMail3PaneWindow.js
var RDF = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService().QueryInterface(Components.interfaces.nsIRDFService);

const MSG_FOLDER_FLAG_OFFLINE = 0x8000000;

function OnLoad()
{	
    if (window.arguments && window.arguments[0]) {
        if (window.arguments[0].msgWindow) {
            gParentMsgWindow = window.arguments[0].msgWindow;
        }
    }
  	   		
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
    gSyncMail = prefs.getBoolPref("mailnews.offline_sync_mail");
    gSyncNews = prefs.getBoolPref("mailnews.offline_sync_news");
    gSendMessage = prefs.getBoolPref("mailnews.offline_sync_send_unsent");
    gWorkOffline = prefs.getBoolPref("mailnews.offline_sync_work_offline");
    document.getElementById("syncMail").checked = gSyncMail;
    document.getElementById("syncNews").checked = gSyncNews;
    document.getElementById("sendMessage").checked = gSendMessage;
    document.getElementById("workOffline").checked = gWorkOffline;

    return true;
}

function syncOkButton()
{

    gSyncMail = document.getElementById("syncMail").checked;
    gSyncNews = document.getElementById("syncNews").checked;	
    gSendMessage = document.getElementById("sendMessage").checked;
    gWorkOffline = document.getElementById("workOffline").checked;

    var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
    prefs.setBoolPref("mailnews.offline_sync_mail", gSyncMail);
    prefs.setBoolPref("mailnews.offline_sync_news", gSyncNews);
    prefs.setBoolPref("mailnews.offline_sync_send_unsent", gSendMessage);
    prefs.setBoolPref("mailnews.offline_sync_work_offline", gWorkOffline);
    
    if( gSyncMail || gSyncNews || gSendMessage || gWorkOffline) {

        var offlineManager = Components.classes["@mozilla.org/messenger/offline-manager;1"].getService(Components.interfaces.nsIMsgOfflineManager);
        if(offlineManager)
            offlineManager.synchronizeForOffline(gSyncNews, gSyncMail, gSendMessage, gWorkOffline, gParentMsgWindow)

    }	

    return true;
}

function OnSelect()
{	  
   top.window.openDialog("chrome://messenger/content/msgSelectOffline.xul", "", "centerscreen,chrome,modal,titlebar,resizable=yes");
   return true;
}

function selectOkButton()
{
    return true;
}

function selectCancelButton()
{	  
    for (var resourceValue in gInitialFolderStates) {
      var resource = RDF.GetResource(resourceValue);
      var folder = resource.QueryInterface(Components.interfaces.nsIMsgFolder);
      if (gInitialFolderStates[resourceValue])
        folder.setFlag(MSG_FOLDER_FLAG_OFFLINE);
      else
        folder.clearFlag(MSG_FOLDER_FLAG_OFFLINE);
    }
    return true;
}

function selectOnLoad()
{
    gMsgWindow = Components.classes[msgWindowContractID].createInstance(Components.interfaces.nsIMsgWindow);
    gMsgWindow.domWindow = window;
    gMsgWindow.rootDocShell.appType = Components.interfaces.nsIDocShell.APP_TYPE_MAIL;

    gAccountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
    gSynchronizeTree = document.getElementById('synchronizeTree');

    SortSynchronizePane('folderNameCol', '?folderTreeNameSort');
} 

function SortSynchronizePane(column, sortKey)
{
    var node = FindInWindow(window, column);
    if(!node) {
        dump('Couldnt find sort column\n');
        return;
    }

    node.setAttribute("sort", sortKey);
    node.setAttribute("sortDirection", "natural");
    var col = gSynchronizeTree.columns[column];
    gSynchronizeTree.view.cycleHeader(col);
}

function FindInWindow(currentWindow, id)
{
    var item = currentWindow.document.getElementById(id);
    if(item)
    return item;

    for(var i = 0; i < currentWindow.frames.length; i++) {
        var frameItem = FindInWindow(currentWindow.frames[i], id);
        if(frameItem)
            return frameItem;
    }

    return null;
}


function onSynchronizeClick(event)
{
    // we only care about button 0 (left click) events
    if (event.button != 0)
      return;

    var row = {}
    var col = {}
    var elt = {}

    gSynchronizeTree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, elt);
    if (row.value == -1)
      return;

    if (elt.value == "twisty") {
        var folderResource = GetFolderResource(gSynchronizeTree, row.value);
        var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);

        if (!(gSynchronizeTree.treeBoxObject.view.isContainerOpen(row.value))) {
            var serverType = GetFolderAttribute(gSynchronizeTree, folderResource, "ServerType");
            // imap is the only server type that does folder discovery
            if (serverType != "imap") return;

            if (GetFolderAttribute(gSynchronizeTree, folderResource, "IsServer") == "true") {
                var server = msgFolder.server;
                server.performExpand(gMsgWindow);
            }
            else {
                var imapFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
                if (imapFolder) {
                  imapFolder.performExpand(gMsgWindow);
                }
            }
        }
    }
    else {
      if (col.value.id == "syncCol") {   
        UpdateNode(GetFolderResource(gSynchronizeTree, row.value), row.value);
      }
    }
}  

function onSynchronizeTreeKeyPress(event)
{
    // for now, only do something on space key
    if (event.charCode != KeyEvent.DOM_VK_SPACE)
      return;

    var treeSelection = gSynchronizeTree.view.selection; 
    for (var i=0;i<treeSelection.getRangeCount();i++) {
      var start = {}, end = {};
      treeSelection.getRangeAt(i,start,end);
      for (var k=start.value;k<=end.value;k++)
        UpdateNode(GetFolderResource(gSynchronizeTree, k), k);
    }
}

function UpdateNode(resource, row)
{
    var folder = resource.QueryInterface(Components.interfaces.nsIMsgFolder);

    if (folder.isServer)
      return;

    if (!(resource.Value in gInitialFolderStates)) {
      gInitialFolderStates[resource.Value] = folder.getFlag(MSG_FOLDER_FLAG_OFFLINE);
    }
    
    folder.toggleFlag(MSG_FOLDER_FLAG_OFFLINE);
}

