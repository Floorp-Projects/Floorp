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

var gIncomingServer;
var gServerType;
var gImapIncomingServer;

function onInit() 
{
    // init values here
    initServerSettings();
    if(gServerType == "nntp") {
        initRetentionSettings();
        initDownloadSettings();
    }

}

function initServerSettings()
{	 
 
    var prefs = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPref);
    var compactFolder = prefs.GetBoolPref("mail.prompt_purge_threshhold");	
    var compactFolderMin = prefs.GetIntPref("mail.purge_threshhold");	
    if (compactFolderMin == null || compactFolderMin == 0 )
        compactFolderMin = 100;
    document.getElementById("offline.notDownload").checked =  gIncomingServer.limitMessageSize;
    document.getElementById("offline.compactFolder").checked = compactFolder;
    document.getElementById("offline.compactFolderMin").setAttribute("value", compactFolderMin);
    if(gIncomingServer.maxMessageSize > 0)
        document.getElementById("offline.notDownloadMin").setAttribute("value", gIncomingServer.maxMessageSize);
    else
        document.getElementById("offline.notDownloadMin").setAttribute("value", "50");

    if(gServerType == "imap") {
        gImapIncomingServer = gIncomingServer.QueryInterface(Components.interfaces.nsIImapIncomingServer);
        document.getElementById("offline.downloadBodiesOnGetNewMail").checked =  gImapIncomingServer.downloadBodiesOnGetNewMail;
        document.getElementById("offline.newFolder").checked =  gImapIncomingServer.offlineDownload;
    }
}
  
function initRetentionSettings()
{

    var retentionSettings =  gIncomingServer.retentionSettings; 

    document.getElementById("nntp.keepUnread").checked =  retentionSettings.keepUnreadMessagesOnly;
    document.getElementById("nntp.removeBody").checked =  retentionSettings.cleanupBodiesByDates;
    document.getElementById("nntp.keepMsg").setAttribute("value", retentionSettings.retainByPreference);
    if(retentionSettings.daysToKeepHdrs > 0)
        document.getElementById("nntp.keepOldMsgMin").setAttribute("value", retentionSettings.daysToKeepHdrs);
    else
        document.getElementById("nntp.keepOldMsgMin").setAttribute("value", "30");
    if(retentionSettings.numHeadersToKeep > 0)
        document.getElementById("nntp.keepNewMsgMin").setAttribute("value", retentionSettings.numHeadersToKeep);
    else 
        document.getElementById("nntp.keepNewMsgMin").setAttribute("value", "30");
    if(retentionSettings.daysToKeepBodies > 0)
        document.getElementById("nntp.removeBodyMin").setAttribute("value", retentionSettings.daysToKeepBodies);
    else
        document.getElementById("nntp.removeBodyMin").setAttribute("value", "30");

    if(retentionSettings.retainByPreference == 1)
        document.getElementById("nntp.keepAllMsg").checked = true;	
    else if(retentionSettings.retainByPreference == 2)
        document.getElementById("nntp.keepOldMsg").checked = true;
    else if(retentionSettings.retainByPreference == 3)
        document.getElementById("nntp.keepNewMsg").checked = true;

}


function initDownloadSettings()
{

    var downloadSettings =  gIncomingServer.downloadSettings;
    document.getElementById("nntp.downloadMsg").checked = downloadSettings.downloadByDate;
    document.getElementById("nntp.downloadUnread").checked = downloadSettings.downloadUnreadOnly;
    if(downloadSettings.ageLimitOfMsgsToDownload > 0)
        document.getElementById("nntp.downloadMsgMin").setAttribute("value", downloadSettings.ageLimitOfMsgsToDownload);
    else
        document.getElementById("nntp.downloadMsgMin").setAttribute("value", "30");
 
}


function onPreInit(account, accountValues)
{

    gServerType = getAccountValue(account, accountValues, "server", "type");
    hideShowControls(gServerType);
    gIncomingServer= account.incomingServer;
    gIncomingServer.type = gServerType;

}


function hideShowControls(type)
{
    
    var controls = document.getElementsByAttribute("hidable", "true");
    var len = controls.length;

    for (var i=0; i<len; i++) {
        var control = controls[i];

        var hideFor = control.getAttribute("hidefor");

        if (!hideFor)
            throw "this should not happen, things that are hidable should have hidefor set";

        var box = getEnclosingContainer(control);

        if (!box)
            throw "this should not happen, things that are hidable should be in a box";

        // hide unsupported server type
        // adding support for hiding multiple server types using hideFor="server1,server2"
        var hideForBool = false;
        var hideForTokens = hideFor.split(",");
        for (var j = 0; j < hideForTokens.length; j++) {
            if (hideForTokens[j] == type) {
                hideForBool = true;
                break;
            }
        }

        if (hideForBool) {
            box.setAttribute("hidden", "true");
        }
        else {
            box.removeAttribute("hidden");
        }
    }
}

function getEnclosingContainer(startNode) {

    var parent = startNode;
    var box;

    while (parent && parent != document) {

    var isContainer = (parent.getAttribute("iscontrolcontainer") == "true");

    if (!box || isContainer)
        box=parent;

    // break out with a controlcontainer
    if (isContainer)
        break;
    parent = parent.parentNode;
    }

    return box;
}


function onClickSelect()
{
   
    top.window.openDialog("chrome://messenger/content/msgSelectOffline.xul", "", "centerscreen,chrome,modal,titlebar,resizable=yes");
    return true;

}

function onSave()
{

    var retentionSettings = new Array;
    var downloadSettings = new Array;

    var prefs = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPref);
    var compactFolderMin = document.getElementById("offline.compactFolderMin").value;
    var compactFolder = document.getElementById("offline.compactFolder").checked;	
 
    gIncomingServer.limitMessageSize = document.getElementById("offline.notDownload").checked;
    gIncomingServer.maxMessageSize = document.getElementById("offline.notDownloadMin").value;

    if(document.getElementById("nntp.keepAllMsg").checked)
        retentionSettings.retainByPreference = 1;		
    else if(document.getElementById("nntp.keepOldMsg").checked)
        retentionSettings.retainByPreference = 2;
    else if(document.getElementById("nntp.keepNewMsg").checked)
        retentionSettings.retainByPreference = 3;

    document.getElementById("nntp.keepMsg").value = retentionSettings.retainByPreference;
    retentionSettings.daysToKeepHdrs = document.getElementById("nntp.keepOldMsgMin").value;
    retentionSettings.daysToKeepBodies = document.getElementById("nntp.removeBodyMin").value;
    retentionSettings.numHeadersToKeep = document.getElementById("nntp.keepNewMsgMin").value;
    retentionSettings.keepUnreadMessagesOnly = document.getElementById("nntp.keepUnread").checked;
    retentionSettings.cleanupBodiesByDates = document.getElementById("nntp.removeBody").checked;

    downloadSettings.downloadByDate = document.getElementById("nntp.downloadMsg").checked;
    downloadSettings.downloadUnreadOnly = document.getElementById("nntp.downloadUnread").checked;
    downloadSettings.ageLimitOfMsgsToDownload = document.getElementById("nntp.downloadMsgMin").value;

    prefs.SetBoolPref("mail.prompt_purge_threshhold", compactFolder);
    prefs.SetIntPref("mail.purge_threshhold", compactFolderMin);
    gIncomingServer.retentionSettings = retentionSettings;
    gIncomingServer.downloadSettings = downloadSettings;

    if (gImapIncomingServer) {
        gImapIncomingServer.downloadBodiesOnGetNewMail = document.getElementById("offline.downloadBodiesOnGetNewMail").checked;
        gImapIncomingServer.offlineDownload = document.getElementById("offline.newFolder").checked;
    }
}

