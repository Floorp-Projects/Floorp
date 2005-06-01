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

var gIncomingServer;
var gServerType;
var gImapIncomingServer;
var gPref = null;
var gLockedPref = null;

function onInit() 
{
    onLockPreference();	

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
 
    document.getElementById("offline.notDownload").checked =  gIncomingServer.limitOfflineMessageSize;
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
    initCommonRetentionSettings(retentionSettings);

    document.getElementById("nntp.removeBody").checked =  retentionSettings.cleanupBodiesByDays;
    if(retentionSettings.daysToKeepBodies > 0)
        document.getElementById("nntp.removeBodyMin").setAttribute("value", retentionSettings.daysToKeepBodies);
    else
        document.getElementById("nntp.removeBodyMin").setAttribute("value", "30");
}


function initDownloadSettings()
{

    var downloadSettings =  gIncomingServer.downloadSettings;
    document.getElementById("nntp.downloadMsg").checked = downloadSettings.downloadByDate;
    document.getElementById("nntp.notDownloadRead").checked = downloadSettings.downloadUnreadOnly;
    if(downloadSettings.ageLimitOfMsgsToDownload > 0)
        document.getElementById("nntp.downloadMsgMin").setAttribute("value", downloadSettings.ageLimitOfMsgsToDownload);
    else
        document.getElementById("nntp.downloadMsgMin").setAttribute("value", "30");
 
}


function onPreInit(account, accountValues)
{

    gServerType = getAccountValue(account, accountValues, "server", "type", null, false);
    hideShowControls(gServerType);
    gIncomingServer= account.incomingServer;
    gIncomingServer.type = gServerType;

    // 10 is OFFLINE_SUPPORT_LEVEL_REGULAR, see nsIMsgIncomingServer.idl
    // currently, there is no offline without diskspace
    var titleStringID = (gIncomingServer.offlineSupportLevel >= 10) ?
     "prefPanel-offline-and-diskspace" : "prefPanel-diskspace";

    var prefBundle = document.getElementById("bundle_prefs");
    var headertitle = document.getElementById("headertitle");
    headertitle.setAttribute('title',prefBundle.getString(titleStringID));

    if (gServerType == "pop3")
    {
      var pop3Server = gIncomingServer.QueryInterface(Components.interfaces.nsIPop3IncomingServer);
      // hide retention settings for deferred accounts
      if (pop3Server.deferredToAccount.length)
      {
        var retentionRadio = document.getElementById("retention.keepMsg");
        retentionRadio.setAttribute("hidden", "true");
        var retentionCheckbox = document.getElementById("retention.keepUnread");
        retentionCheckbox.setAttribute("hidden", "true");
        var retentionLabel = document.getElementById("retentionDescription");
        retentionLabel.setAttribute("hidden", "true");
      }
    }
}

function onClickSelect()
{
   
    top.window.openDialog("chrome://messenger/content/msgSelectOffline.xul", "", "centerscreen,chrome,modal,titlebar,resizable=yes");
    return true;

}

function onSave()
{

    var downloadSettings = new Array;

    gIncomingServer.limitOfflineMessageSize = document.getElementById("offline.notDownload").checked;
    gIncomingServer.maxMessageSize = document.getElementById("offline.notDownloadMin").value;

	var retentionSettings = saveCommonRetentionSettings();

    retentionSettings.daysToKeepBodies = document.getElementById("nntp.removeBodyMin").value;
    retentionSettings.cleanupBodiesByDays = document.getElementById("nntp.removeBody").checked;

    downloadSettings.downloadByDate = document.getElementById("nntp.downloadMsg").checked;
    downloadSettings.downloadUnreadOnly = document.getElementById("nntp.notDownloadRead").checked;
    downloadSettings.ageLimitOfMsgsToDownload = document.getElementById("nntp.downloadMsgMin").value;

    gIncomingServer.retentionSettings = retentionSettings;
    gIncomingServer.downloadSettings = downloadSettings;

    if (gImapIncomingServer) {
        gImapIncomingServer.downloadBodiesOnGetNewMail = document.getElementById("offline.downloadBodiesOnGetNewMail").checked;
        gImapIncomingServer.offlineDownload = document.getElementById("offline.newFolder").checked;
    }
}

// Does the work of disabling an element given the array which contains xul id/prefstring pairs.
// Also saves the id/locked state in an array so that other areas of the code can avoid
// stomping on the disabled state indiscriminately.
function disableIfLocked( prefstrArray )
{
    if (!gLockedPref)
      gLockedPref = new Array;

    for (var i=0; i<prefstrArray.length; i++) {
        var id = prefstrArray[i].id;
        var element = document.getElementById(id);
        if (gPref.prefIsLocked(prefstrArray[i].prefstring)) {
            element.disabled = true;
            gLockedPref[id] = true;
        } else {
            element.removeAttribute("disabled");
            gLockedPref[id] = false;
        }
    }
}

// Disables xul elements that have associated preferences locked.
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
    var allPrefElements = [
      { prefstring:"offline_download", id:"offline.newFolder"},
      { prefstring:"download_bodies_on_get_new_mail",
                           id:"offline.downloadBodiesOnGetNewMail"},
      { prefstring:"limit_offline_message_size", id:"offline.notDownload"},
      { prefstring:"max_size", id:"offline.notDownloadMin"},
      { prefstring:"downloadUnreadOnly", id:"nntp.notDownloadRead"},
      { prefstring:"downloadByDate", id:"nntp.downloadMsg"},
      { prefstring:"ageLimit", id:"nntp.downloadMsgMin"},
      { prefstring:"retainBy", id:"retention.keepMsg"},
      { prefstring:"daysToKeepHdrs", id:"retention.keepOldMsgMin"},
      { prefstring:"numHdrsToKeep", id:"retention.keepNewMsgMin"},
      { prefstring:"keepUnreadOnly", id:"retention.keepUnread"},
      { prefstring:"daysToKeepBodies", id:"nntp.removeBodyMin"},
      { prefstring:"cleanupBodies", id:"nntp.removeBody" },
      { prefstring:"disable_button.selectFolder", id:"selectNewsgroupsButton"},
      { prefstring:"disable_button.selectFolder", id:"selectImapFoldersButton"}
    ];

    finalPrefString = initPrefString + "." + gIncomingServer.key + ".";
    gPref = prefService.getBranch(finalPrefString);

    disableIfLocked( allPrefElements );
} 

function onCheckItem(broadcasterElementId, checkElementId)
{
    var broadcaster = document.getElementById(broadcasterElementId);
    var checked = document.getElementById(checkElementId).checked;
    if(checked && !gLockedPref[checkElementId] ) {
        broadcaster.removeAttribute("disabled");
    }
    else {
        broadcaster.setAttribute("disabled", "true");
    }

} 

