# -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Scott MacGregor <mscott@mozilla.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var MailOfflineMgr = {
  offlineManager: null,
  offlineBundle: null,
  
  init: function()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"]
              .getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "network:offline-status-changed", false);
    
    this.offlineManager = Components.classes["@mozilla.org/messenger/offline-manager;1"]                 
                        .getService(Components.interfaces.nsIMsgOfflineManager);
    this.offlineBundle = document.getElementById("bundle_offlinePrompts");
    
    // initialize our offline state UI
    this.updateOfflineUI(!this.isOnline());
  },
  
  uninit: function()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"]
              .getService(Components.interfaces.nsIObserverService);
    os.removeObserver(this, "network:offline-status-changed");
  },
  
  /** 
   * @return true if we are online
   */
   isOnline: function()
   {
      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                             .getService(Components.interfaces.nsIIOService);
      return (!ioService.offline);
   },
   
  /**
   * Toggles the online / offline state, initiated by the user. Depending on user settings
   * we may prompt the user to send unsent messages when going online or to download messages for
   * offline use when going offline.
   */
  toggleOfflineStatus: function()
  {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService2);
    // the offline manager(goOnline and synchronizeForOffline) actually does the dirty work of 
    // changing the offline state with the networking service.
    if (!this.isOnline()) 
    {
      var prefSendUnsentMessages = gPrefBranch.getIntPref("offline.send.unsent_messages");
      // 0 == Ask, 1 == Always Send, 2 == Never Send
      var sendUnsentMessages = (prefSendUnsentMessages == 0 && this.haveUnsentMessages() 
                                && this.confirmSendUnsentMessages()) || prefSendUnsentMessages == 1;
      this.offlineManager.goOnline(sendUnsentMessages, true /* playbackOfflineImapOperations */, msgWindow);
      
      // resume managing offline status now that we are going back online.
      ioService.manageOfflineStatus = gPrefBranch.getBoolPref("offline.autoDetect");      
    }
    else // going offline
    {               
      // Stop automatic management of the offline status since the user as decided to go offline.
      ioService.manageOfflineStatus = false;
      var prefDownloadMessages = gPrefBranch.getIntPref("offline.download.download_messages");
      // 0 == Ask, 1 == Always Download, 2 == Never Download
      var downloadForOfflineUse = (prefDownloadMessages == 0 && this.confirmDownloadMessagesForOfflineUse())
                                  || prefDownloadMessages == 1;
      this.offlineManager.synchronizeForOffline(downloadForOfflineUse, downloadForOfflineUse, false, true, msgWindow);
    }
  },
  
  observe: function (aSubject, aTopic, aState)
  {
    if (aTopic == "network:offline-status-changed")
      this.mailOfflineStateChanged(aState == "offline");
  },
  
  /** 
   * @return true if there are unsent messages
   */
  haveUnsentMessages: function()
  {
    try
    {
      var am = Components.classes["@mozilla.org/messenger/account-manager;1"]
                 .getService(Components.interfaces.nsIMsgAccountManager);
      var msgSendlater = Components.classes["@mozilla.org/messengercompose/sendlater;1"]
                           .getService(Components.interfaces.nsIMsgSendLater);
      var identitiesCount, allIdentities, currentIdentity, numMessages, msgFolder;

      if (am) 
      { 
        allIdentities = am.allIdentities;
        identitiesCount = allIdentities.Count();
        for (var i = 0; i < identitiesCount; i++) 
        {
          currentIdentity = allIdentities.QueryElementAt(i, Components.interfaces.nsIMsgIdentity);
          msgFolder = msgSendlater.getUnsentMessagesFolder(currentIdentity);
          if(msgFolder) 
          {
            // if true, descends into all subfolders 
            numMessages = msgFolder.getTotalMessages(false);
            if(numMessages > 0) 
              return true;
          }
        } 
      }
    }
    catch(ex) {
    }
    
    return false; 
  },
  
  /**
   * open the offline panel in the account manager for the currently loaded
   * account.
   */
  openOfflineAccountSettings: function()
  {
    window.parent.MsgAccountManager('am-offline.xul');
  },
  
  /** 
   * Prompt the user about going online to send unsent messages, and then send them
   * if appropriate. Puts the app back into online mode.
   * 
   * @param aMsgWindow the msg window to be used when going online
   */   
  goOnlineToSendMessages: function(aMsgWindow)
  {
    const nsIPS = Components.interfaces.nsIPromptService;
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPS);
    var goOnlineToSendMsgs = promptService.confirm(window, 
                                                   this.offlineBundle.getString('sendMessagesOfflineWindowTitle1'), 
                                                   this.offlineBundle.getString('sendMessagesOfflineLabel1'));
    if (goOnlineToSendMsgs)
      this.offlineManager.goOnline(true /* send unsent messages*/, false, aMsgWindow);
  },
  
  /**
   * Prompts the user to confirm sending of unsent messages. This is different from 
   * goOnlineToSendMessages which involves going online to send unsent messages.
   *
   * @return true if the user wants to send unsent messages
   */
  confirmSendUnsentMessages: function()
  {
    const nsIPS = Components.interfaces.nsIPromptService;
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPS);
    var alwaysAsk = {value: true};
    var sendUnsentMessages = promptService.confirmEx(window, 
                                                     this.offlineBundle.getString('sendMessagesWindowTitle1'), 
                                                     this.offlineBundle.getString('sendMessagesLabel1'),
                                                     nsIPS.STD_YES_NO_BUTTONS,
                                                     null, null, null,
                                                     this.offlineBundle.getString('sendMessagesCheckboxLabel1'), 
                                                     alwaysAsk) == 0 ? true : false;
                             
    // if the user changed the ask me setting then update the global pref based on their yes / no answer
    if (!alwaysAsk.value)
      gPrefBranch.setIntPref("offline.send.unsent_messages", sendUnsentMessages ? 1 : 2);
    return sendUnsentMessages;
  },
   
  /** 
   * Should we send unsent messages? Based on the value of
   * offline.send.unsent_messages, this method may prompt the user.
   * @return true if we should send unsent messages
   */
  shouldSendUnsentMessages: function()
  {
    var sendUnsentWhenGoingOnlinePref = gPrefBranch.getIntPref("offline.send.unsent_messages");
    if(sendUnsentWhenGoingOnlinePref == 2) // never send
      return false;

    // if we we have unsent messages, then honor the offline.send.unsent_messages pref.
    else if (this.haveUnsentMessages())
    {
      if ((sendUnsentWhenGoingOnlinePref == 0 && this.confirmSendUnsentMessages())
           || sendUnsentWhenGoingOnlinePref == 1)
        return true;
    }
    return false;
  },
   
  /**
   * Prompts the user to download messages for offline use before going offline.
   * May update the value of offline.download.download_messages
   *
   * @return true if the user wants to download messages for offline use.
   */   
  confirmDownloadMessagesForOfflineUse: function()
  {
    const nsIPS = Components.interfaces.nsIPromptService;
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPS);
    var alwaysAsk = {value: true};
    var downloadMessages = promptService.confirmEx(window, 
                                                   this.offlineBundle.getString('downloadMessagesWindowTitle1'), 
                                                   this.offlineBundle.getString('downloadMessagesLabel1'),
                                                   nsIPS.STD_YES_NO_BUTTONS,
                                                   null, null, null,
                                                   this.offlineBundle.getString('downloadMessagesCheckboxLabel1'), 
                                                   alwaysAsk) == 0 ? true : false;
                             
    // if the user changed the ask me setting then update the global pref based on their yes / no answer
    if (!alwaysAsk.value)
      gPrefBranch.setIntPref("offline.download.download_messages", downloadMessages ? 1 : 2);
    return downloadMessages;      
  },
  
  /** 
   *  Get New Mail When Offline
   *  Prompts the user about going online in order to download new messages. 
   *  Based on the response, will move us back to online mode.
   *
   * @return true if the user confirms going online.
   */
  getNewMail: function()
  {
    const nsIPS = Components.interfaces.nsIPromptService;
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPS);
    var goOnline =  promptService.confirm(window, 
                                          this.offlineBundle.getString('getMessagesOfflineWindowTitle1'), 
                                          this.offlineBundle.getString('getMessagesOfflineLabel1'));    
    if (goOnline)
      this.offlineManager.goOnline(this.shouldSendUnsentMessages(),
                                   false /* playbackOfflineImapOperations */, msgWindow);
    return goOnline;
  },
   
  /** 
   * Private helper method to update the state of the Offline menu item
   * and the offline status bar indicator
   */
  updateOfflineUI: function(aIsOffline)
  {
    document.getElementById('goOfflineMenuItem').setAttribute("checked", aIsOffline);
    var statusBarPanel = document.getElementById('offline-status');
    if (aIsOffline)
    {
      statusBarPanel.setAttribute("offline", "true");
      statusBarPanel.setAttribute("tooltiptext", this.offlineBundle.getString("offlineTooltip"));
    }
    else
    {
      statusBarPanel.removeAttribute("offline");
      statusBarPanel.setAttribute("tooltiptext", this.offlineBundle.getString("onlineTooltip"));
    }    
  },   

  /**
   * private helper method called whenever we detect a change to the offline state
   */ 
  mailOfflineStateChanged: function (aGoingOffline)
  {
    gFolderJustSwitched = true;
    this.updateOfflineUI(aGoingOffline);
  }
};
