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
    initRetentionSettings();
    initDownloadSettings();

    onCheckItem("bc_notDownload", "offline.notDownload");
    onCheckItem("bc_downloadMsg", "nntp.downloadMsg");
    onCheckItem("bc_removeBody", "nntp.removeBody");
    onCheckKeepMsg();
}

function initServerSettings()
{	 
 
    document.getElementById("offline.notDownload").checked =  gIncomingServer.limitMessageSize;
    if(gIncomingServer.maxMessageSize > 0)
        document.getElementById("offline.notDownloadMin").setAttribute("value", gIncomingServer.maxMessageSize);
    else
        document.getElementById("offline.notDownloadMin").setAttribute("value", "50");

    if(gServerType == "imap") {
        gImapIncomingServer = gIncomingServer.QueryInterface(Components.interfaces.nsIImapIncomingServer);
        document.getElementById("offline.downloadBodiesOnGetNewMail").checked =  gImapIncomingServer.downloadBodiesOnGetNewMail;
        document.getElementById("offline.newFolder").checked =  gImapIncomingServer.offlineDownload;
    }
   // onLockPreference();	
}
  
function initRetentionSettings()
{

    var retentionSettings =  gIncomingServer.retentionSettings; 

    document.getElementById("nntp.keepUnread").checked =  retentionSettings.keepUnreadMessagesOnly;
    document.getElementById("nntp.removeBody").checked =  retentionSettings.cleanupBodiesByDays;
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

    switch(retentionSettings.retainByPreference)
    {		
        case 1:
        document.getElementById("nntp.keepAllMsg").checked = true;	
            break;
        case 2:   
        document.getElementById("nntp.keepOldMsg").checked = true;
            break;
        case 3:    
        document.getElementById("nntp.keepNewMsg").checked = true;
            break;
        default:
            document.getElementById("nntp.keepAllMsg").checked = true;
    }  
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

    var titleStringID;
    // 10 is OFFLINE_SUPPORT_LEVEL_REGULAR, see nsIMsgIncomingServer.idl
    // currently, there is no offline without diskspace
    if (gIncomingServer.offlineSupportLevel >= 10) {
      titleStringID = "prefPanel-offline-and-diskspace";
    }
    else {
      titleStringID = "prefPanel-diskspace";
    }

    var prefBundle = document.getElementById("bundle_prefs");
    var headertitle = document.getElementById("headertitle");
    headertitle.setAttribute('title',prefBundle.getString(titleStringID));
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
    retentionSettings.cleanupBodiesByDays = document.getElementById("nntp.removeBody").checked;

    downloadSettings.downloadByDate = document.getElementById("nntp.downloadMsg").checked;
    downloadSettings.downloadUnreadOnly = document.getElementById("nntp.downloadUnread").checked;
    downloadSettings.ageLimitOfMsgsToDownload = document.getElementById("nntp.downloadMsgMin").value;

    gIncomingServer.retentionSettings = retentionSettings;
    gIncomingServer.downloadSettings = downloadSettings;

    if (gImapIncomingServer) {
        gImapIncomingServer.downloadBodiesOnGetNewMail = document.getElementById("offline.downloadBodiesOnGetNewMail").checked;
        gImapIncomingServer.offlineDownload = document.getElementById("offline.newFolder").checked;
    }
}
var gPref = null;

function disableIfLocked( prefstrArray )
{
    for (i=0; i<prefstrArray.length; i++) {
        var element = document.getElementById(prefstrArray[i].id);
        if (gPref.prefIsLocked(prefstrArray[i].prefstring))
            element.disabled = true;
        else
            element.removeAttribute("disabled");
    }
}

function onLockPreference()
{
    var isDownloadLocked = false;
    var isGetNewLocked = false;
    var initPrefString = "mail.server"; 
    var finalPrefString; 

    var prefService = Components.classes["@mozilla.org/preferences-service;1"];
    prefService = prefService.getService();
    prefService = prefService.QueryInterface(Components.interfaces.nsIPrefService);

    // This panel does not use the code in AccountManager.js to handle
    // the load/unload/disable.  keep in mind new prefstrings and changes
    // to code in AccountManager, and update these as well.
    var prefElements = [
      { prefstring:"offline_download", id:"offline.newFolder"},
      { prefstring:"download_bodies_on_get_new_mail",
                           id:"offline.downloadBodiesOnGetNewMail"},
      { prefstring:"limit_message_size", id:"offline.notDownload"},
      { prefstring:"max_size", id:"offline.notDownloadMin"},
      { prefstring:"downloadUnreadOnly", id:"nntp.downloadUnread"},
      { prefstring:"downloadByDate", id:"nntp.downloadMsg"},
      { prefstring:"ageLimit", id:"nntp.downloadMsgMin"},
      { prefstring:"retainBy", id:"nntp.keepMsg"},
      { prefstring:"daysToKeepHdrs", id:"nntp.keepOldMsgMin"},
      { prefstring:"numHdrsToKeep", id:"nntp.keepNewMsgMin"},
      { prefstring:"keepUnreadOnly", id:"nntp.keepUnread"},
      { prefstring:"daysToKeepBodies", id:"nntp.removeBodyMin"},
      { prefstring:"disable_button.selectFolder", id:"selectFolderButton"}
    ];

    finalPrefString = initPrefString + "." + gIncomingServer.key + ".";
    gPref = prefService.getBranch(finalPrefString);

    disableIfLocked( prefElements );

/*  This element doesn't currently work.  When it does and the prefstring
is known, it needs to be added to the array.  See bugs 91560 and 79561
    { prefstring:"", id:"nntp.removeBody" }
*/
} 

function onCheckItem(broadcasterElementId, checkElementId)
{
    var broadcaster = document.getElementById(broadcasterElementId);
    var checked = document.getElementById(checkElementId).checked;
    if(checked) {
        broadcaster.removeAttribute("disabled");
    }
    else {
        broadcaster.setAttribute("disabled", "true");
    }

} 

function onCheckKeepMsg()
{
    var broadcaster_keepMsg = document.getElementById("bc_keepMsg");
    var checkedOld = document.getElementById("nntp.keepOldMsg").checked;
    var checkedNew = document.getElementById("nntp.keepNewMsg").checked;
    var checkedAll = document.getElementById("nntp.keepAllMsg").checked;
    if(checkedAll) {
        broadcaster_keepMsg.setAttribute("disabled", "true");
    }
    else if(checkedOld) {
        document.getElementById("nntp.keepOldMsgMin").removeAttribute("disabled");
        document.getElementById("nntp.keepNewMsgMin").setAttribute("disabled", "true");
    }
    else if(checkedNew) {
        document.getElementById("nntp.keepNewMsgMin").removeAttribute("disabled");
        document.getElementById("nntp.keepOldMsgMin").setAttribute("disabled", "true");
    }


}

