/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributors:
 *   dianesun@netscape.com
 */

var gSyncMail = false;
var gSyncNews = false;
var gSendMessage = true;
var gWorkOffline = false;
var gSynchronizeTree = null;
var gAccountManager = null;
var gParentMsgWindow;
const MSG_FOLDER_FLAG_OFFLINE = 0x8000000;

function OnLoad()
{	
    if (window.arguments && window.arguments[0]) {
        if (window.arguments[0].msgWindow) {
            gParentMsgWindow = window.arguments[0].msgWindow;
        }
    }
  	   		
    doSetOKCancel(syncOkButton, syncCancelButton);
    var prefs = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPref);
    gSyncMail = prefs.GetBoolPref("mailnews.offline_sync_mail");
    gSyncNews = prefs.GetBoolPref("mailnews.offline_sync_news");
    gSendMessage = prefs.GetBoolPref("mailnews.offline_sync_send_unsent");
    gWorkOffline = prefs.GetBoolPref("mailnews.offline_sync_work_offline");
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

    var prefs = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPref);
    prefs.SetBoolPref("mailnews.offline_sync_mail", gSyncMail);
    prefs.SetBoolPref("mailnews.offline_sync_news", gSyncNews);
    prefs.SetBoolPref("mailnews.offline_sync_send_unsent", gSendMessage);
    prefs.SetBoolPref("mailnews.offline_sync_work_offline", gWorkOffline);
    
    if( gSyncMail || gSyncNews || gSendMessage || gWorkOffline) {

        var offlineManager = Components.classes["@mozilla.org/messenger/offline-manager;1"].getService(Components.interfaces.nsIMsgOfflineManager);
        if(offlineManager)
            offlineManager.synchronizeForOffline(gSyncNews, gSyncMail, gSendMessage, gWorkOffline, gParentMsgWindow)

    }	

    return true;
}

function syncCancelButton()
{
    //dump("in syncCancelButton()\n");
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
    return true;
}

function selectOnLoad()
{
    doSetOKCancel(selectOkButton,selectCancelButton);

    msgWindow = Components.classes[msgWindowContractID].createInstance(Components.interfaces.nsIMsgWindow);
    msgWindow.SetDOMWindow(window);

    gAccountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
    gSynchronizeTree = document.getElementById('synchronizetree');

    // Add folderDataSource and accountManagerDataSource to tree datatbase 
    var datasourceContractIDPrefix = "@mozilla.org/rdf/datasource;1?name=";
    var accountManagerDSContractID = datasourceContractIDPrefix + "msgaccountmanager";
    var folderDSContractID         = datasourceContractIDPrefix + "mailnewsfolders";
    var accountManagerDataSource = Components.classes[accountManagerDSContractID].createInstance();
    var folderDataSource         = Components.classes[folderDSContractID].createInstance();
    accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
    folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);

    gSynchronizeTree.database.AddDataSource(accountManagerDataSource);
    gSynchronizeTree.database.AddDataSource(folderDataSource);
    gSynchronizeTree.setAttribute('ref', 'msgaccounts:/');

    SetServerOpen();
    SortFolder('FolderColumn', 'http://home.netscape.com/NC-rdf#FolderTreeName');
    LoadSyncTree();
} 

function SortFolder(column, sortKey)
{
    var node = FindInWindow(window, column);
    if(!node) {
        dump('Couldnt find sort column\n');
        return false;
    }
    SortColumn(node, sortKey, null, null);
    node.setAttribute("sortActive", "false");
    return true;
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


function LoadSyncTree()
{
    var allServers = gAccountManager.allServers;
    for (var i=0;i<allServers.Count();i++)	{

        var currentServer = allServers.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
        var rootURI = currentServer.serverURI;
        var rootFolder = currentServer.RootFolder;

        var rootMsgFolder = rootFolder.QueryInterface(Components.interfaces.nsIMsgFolder);
        var rootFolderResource = rootMsgFolder.QueryInterface(Components.interfaces.nsIRDFResource);

        if(currentServer.type == "imap"){			
            var imapFolder = rootMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder)
            imapFolder.performExpand(msgWindow);
        }
        else if(currentServer.type == "nntp") {			
            currentServer.PerformExpand(msgWindow);
        }
    }
}

function SetServerOpen()
{
    if ( gSynchronizeTree && gSynchronizeTree.childNodes ) 	{
        for ( var i = gSynchronizeTree.childNodes.length - 1; i >= 0; i-- ) {
            var treechild = gSynchronizeTree.childNodes[i];
            if (treechild.localName == 'treechildren') 	{
                var treeitems = treechild.childNodes;				
                for ( var j = treeitems.length - 1; j >= 0; j--)  {
                    var isServer = treeitems[j].getAttribute("IsServer");
                    if (isServer) { 
                        if(treeitems[j].getAttribute('ServerType') == "imap" ||
                           treeitems[j].getAttribute('ServerType') == "nntp") {
                            treeitems[j].setAttribute('open', true);							 							 
                        }
                        else {							
                            treeitems[j].setAttribute('open', false);					
                        }
                    }
                }
            }
        }
    }
}

function onSpaceClick(event)
{
    // if click checkbox,  reverse selected status,
    var t = event.originalTarget;
    if (t.localName == "checkbox") {
        var node = t.parentNode.parentNode.parentNode;
        if (node.localName == "treeitem") {
            var nodeType = node.getAttribute("ServerType");
            if(nodeType =="imap" || nodeType =="nntp")
                UpdateNode(node, t);
        }
    }
}  

function UpdateNode(node, target)
{
    var folder = null;
    var uri = node.getAttribute("id");
    if(uri)
        folder = GetMsgFolderFromUri(uri);
    if(folder) {

        if(folder.isServer) {			
            target.setAttribute('value', false); 					
            return;
        }

        var oldFlag = folder.getFlag(MSG_FOLDER_FLAG_OFFLINE);
        if(!oldFlag) {
            folder.setFlag(MSG_FOLDER_FLAG_OFFLINE);
        }
        else {
            folder.clearFlag(MSG_FOLDER_FLAG_OFFLINE);
        }
    } 
}



