/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Bienvenu <bienvenu@netscape.com>
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

var gMailOfflinePrefs = null;
var gOfflinePromptsBundle;
var gPromptService;
var gOfflineManager;


function MailOfflineStateChanged(goingOffline)
{
  // tweak any mail UI here that needs to change when we go offline or come back online
  gFolderJustSwitched = true;
}

function MsgSettingsOffline()
{
    window.parent.MsgAccountManager('am-offline.xul');
}

// Init PrefsService
function GetMailOfflinePrefs()
{
  // Store the prefs object
  try {
    var prefsService = Components.classes["@mozilla.org/preferences-service;1"];
    if (prefsService)
    prefsService = prefsService.getService();
    if (prefsService)
    gMailOfflinePrefs = prefsService.QueryInterface(Components.interfaces.nsIPrefBranch);

    if (!gMailOfflinePrefs)
    dump("failed to get prefs service!\n");
  }
  catch(ex) {
    dump("failed to get prefs service!\n");
  }
}

// Check for unsent messages
function CheckForUnsentMessages()
{
  try
  {
    var am = Components.classes["@mozilla.org/messenger/account-manager;1"]
                 .getService(Components.interfaces.nsIMsgAccountManager);
    var msgSendlater = Components.classes["@mozilla.org/messengercompose/sendlater;1"]
                 .getService(Components.interfaces.nsIMsgSendLater);
    var identitiesCount, allIdentities, currentIdentity, numMessages, msgFolder;

    if(am) { 
      allIdentities = am.allIdentities;
      identitiesCount = allIdentities.Count();
      for (var i = 0; i < identitiesCount; i++) {
        currentIdentity = allIdentities.QueryElementAt(i, Components.interfaces.nsIMsgIdentity);
        msgFolder = msgSendlater.getUnsentMessagesFolder(currentIdentity);
        if(msgFolder) {
          // if true, descends into all subfolders 
          numMessages = msgFolder.getTotalMessages(false);
          if(numMessages > 0) return true;
        }
      } 
    }
  }
  catch(ex) {
  }
  return false;
}

// Init nsIPromptService & strings.
function InitPrompts()
{
  if(!gPromptService) {
    gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
  }
  if (!gOfflinePromptsBundle) 
    gOfflinePromptsBundle = document.getElementById("bundle_offlinePrompts");
}

// prompt for sending messages while going online, and go online.
function PromptSendMessages()
{
  InitPrompts();
  InitServices();

  if (gPromptService) {
    var checkValue = {value:true};
    var buttonPressed = gPromptService.confirmEx(window, 
                          gOfflinePromptsBundle.getString('sendMessagesWindowTitle'), 
                          gOfflinePromptsBundle.getString('sendMessagesLabel'),
                          gPromptService.BUTTON_TITLE_IS_STRING * (gPromptService.BUTTON_POS_0 + 
                            gPromptService.BUTTON_POS_1 + gPromptService.BUTTON_POS_2),
                          gOfflinePromptsBundle.getString('sendMessagesSendButtonLabel'),
                          gOfflinePromptsBundle.getString('sendMessagesCancelButtonLabel'),
                          gOfflinePromptsBundle.getString('sendMessagesNoSendButtonLabel'),
                          gOfflinePromptsBundle.getString('sendMessagesCheckboxLabel'), 
                          checkValue);
    if(buttonPressed == 0) {
      gMailOfflinePrefs.setIntPref("offline.send.unsent_messages", !checkValue.value);
      gOfflineManager.goOnline(true, true, msgWindow);
      return true;
    }
    if(buttonPressed == 1) {
      return false;
    }
    if(buttonPressed == 2) {
      gMailOfflinePrefs.setIntPref("offline.send.unsent_messages", 2*!checkValue.value);
      gOfflineManager.goOnline(false, true, msgWindow);
      return true;
    }
  }
  return false;
}

// prompt for downlading messages while going offline, and synchronise
function PromptDownloadMessages()
{
  InitPrompts();
  InitServices();

  if(gPromptService) {
    var checkValue = {value:true};
    var buttonPressed = gPromptService.confirmEx(window, 
                          gOfflinePromptsBundle.getString('downloadMessagesWindowTitle'), 
                          gOfflinePromptsBundle.getString('downloadMessagesLabel'),
                          gPromptService.BUTTON_TITLE_IS_STRING * (gPromptService.BUTTON_POS_0 + 
                            gPromptService.BUTTON_POS_1 + gPromptService.BUTTON_POS_2),
                          gOfflinePromptsBundle.getString('downloadMessagesDownloadButtonLabel'),
                          gOfflinePromptsBundle.getString('downloadMessagesCancelButtonLabel'),
                          gOfflinePromptsBundle.getString('downloadMessagesNoDownloadButtonLabel'), 
                          gOfflinePromptsBundle.getString('downloadMessagesCheckboxLabel'), 
                          checkValue);
    if(buttonPressed == 0) {
      gMailOfflinePrefs.setIntPref("offline.download.download_messages", !checkValue.value);
      gOfflineManager.synchronizeForOffline(true, true, false, true, msgWindow);
      return true;
    }
    if(buttonPressed == 1) {
      return false;
    }
    if(buttonPressed == 2) {
      gMailOfflinePrefs.setIntPref("offline.download.download_messages", 2*!checkValue.value);
      gOfflineManager.synchronizeForOffline(false, false, false, true, msgWindow);
      return true;
    }
  }
  return false;
}

// online?
function CheckOnline()
{
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                         .getService(Components.interfaces.nsIIOService);
  return (!ioService.offline);
}

// Init Pref Service & Offline Manager
function InitServices()
{
  if (!gMailOfflinePrefs) 
    GetMailOfflinePrefs();

  if (!gOfflineManager) 
    GetOfflineMgrService();
}

// Init Offline Manager
function GetOfflineMgrService()
{
  if (!gOfflineManager) {
    gOfflineManager = Components.classes["@mozilla.org/messenger/offline-manager;1"]                 
        .getService(Components.interfaces.nsIMsgOfflineManager);
  }
}

// This function must always return false to prevent toggling of offline state because
// we change the offline state ourselves
function MailCheckBeforeOfflineChange()
{
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);

  var goingOnline = ioService.offline;
  var bundle = srGetStrBundle("chrome://communicator/locale/utilityOverlay.properties");

//  messenger.SetWindow(window, msgWindow);

  InitServices();

  var prefSendUnsentMessages = gMailOfflinePrefs.getIntPref("offline.send.unsent_messages");
  var prefDownloadMessages   = gMailOfflinePrefs.getIntPref("offline.download.download_messages");

  if(goingOnline) {
    switch(prefSendUnsentMessages) { 
    case 0:
      if(CheckForUnsentMessages()) { 
        if(! PromptSendMessages()) 
          return false;
      }
      else 
        gOfflineManager.goOnline(false /* sendUnsentMessages */, 
                                 true /* playbackOfflineImapOperations */, 
                                 msgWindow);
      break;
    case 1:
      gOfflineManager.goOnline(CheckForUnsentMessages() /* sendUnsentMessages */, 
                               true  /* playbackOfflineImapOperations */, 
                               msgWindow);
      break;
    case 2:
      gOfflineManager.goOnline(false /* sendUnsentMessages */, 
                               true /* playbackOfflineImapOperations */, 
                               msgWindow);
      break;
    }
  }
  else {
    // going offline
    switch(prefDownloadMessages) {	
      case 0:
        if(! PromptDownloadMessages()) return false;
      break;
      case 1:
        // download news, download mail, send unsent messages, go offline when done, msg window
        gOfflineManager.synchronizeForOffline(true, true, false, true, msgWindow);
        break;
      case 2:
        // download news, download mail, send unsent messages, go offline when done, msg window
        gOfflineManager.synchronizeForOffline(false, false, false, true, msgWindow);
        break;
    }
  }
  return false;
}

