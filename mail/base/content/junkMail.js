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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Seth Spitzer <sspitzer@netscape.com>
#   Dan Mosedale <dmose@netscape.com>
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

var gSpamSettings = {};
var gCurrentServer;
var gMessengerBundle;

// pref service is global
var gPrefService = null;

const kJunkOnLocalFolderURI = "mailbox://nobody@Local%20Folders/Junk";

function onJunkMailLoad()
{
  gMessengerBundle = document.getElementById("bundle_messenger");
  if (window.arguments && window.arguments[0]) {
    setupForAccountFromFolder(window.arguments[0].folder ? window.arguments[0].folder.URI : null);
  }

  // initialize prefs
  gPrefService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

  // initialize the sanitize junk mail html check box
  document.getElementById('renderWithSafeHtml').checked = gPrefService.getBoolPref('mailnews.display.sanitizeJunkMail');
}

function onServerClick(event)
{ 
  if (gCurrentServer.serverURI == event.target.id)
    return;

  // before we set the UI for the new server,
  // save off the old one
  storeSettings(gSpamSettings[gCurrentServer.key].settings, gCurrentServer.spamSettings.loggingEnabled);

  // set up the UI for the server
  setupForAccountFromFolder(event.target.id);
}

function setupForAccountFromFolder(aURI)
{
  try {
    if (!aURI)
      throw "this can happen if no folder is selected in the folder pane"
    var msgFolder = GetMsgFolderFromUri(aURI, false);
    gCurrentServer = msgFolder.server;

    if (gCurrentServer.type == "nntp")
      throw "this can happen if the selected folder (or account) doesn't have junk controls (news)"
  }
  catch (ex) {
    // get server for default account
    // XXX TODO
    // edge cases to worry about later:
    // what if there is no default account? 
    // what if default account is of a type where canGetIncomingMessages == true?
    // what if no accounts are of a type where canGetIncomingMessages == true?
    var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"]
               .getService(Components.interfaces.nsIMsgAccountManager);
    var account = accountManager.defaultAccount;
    gCurrentServer = account.incomingServer;
  }

  var obj;
  var key = gCurrentServer.key;

  if (key in gSpamSettings) {
    obj = gSpamSettings[key];
  }
  else {
    // get and clone spam settings for this server
    // we clone because if the users cancels we are going to throw away the changes
    var settings = Components.classes["@mozilla.org/messenger/spamsettings;1"].createInstance(Components.interfaces.nsISpamSettings);
    settings.clone(gCurrentServer.spamSettings);
    obj = {server: gCurrentServer, settings: settings}; 
    gSpamSettings[key] = obj;
  } 

  // select server in the menulist
  var serverList = document.getElementById("server");
  var menuitems = serverList.getElementsByAttribute("id", obj.server.serverURI);
  serverList.selectedItem = menuitems[0];

  // set up the UI for this server
  // set up the level checkbox
  document.getElementById("level").checked = (obj.settings.level > 0);

  // set up the junk mail folder picker
  document.getElementById("moveOnSpam").checked = obj.settings.moveOnSpam;
  document.getElementById("moveTargetMode").selectedItem = document.getElementById("moveTargetMode" + obj.settings.moveTargetMode);

  // the default account should be the current account
  // unless you can't create a folder on that server
  // or search on that account (for purge)
  // in which case, use Local Folders
  var defaultAccountURI = obj.server.canCreateFoldersOnServer && obj.server.canSearchMessages ? obj.server.serverURI : "mailbox://nobody@Local%20Folders";
  // if there is a target account, use it, else use the default account
  SetFolderPicker(obj.settings.actionTargetAccount ? obj.settings.actionTargetAccount : defaultAccountURI, "actionTargetAccount");
  
  // if there is a target account, use it, else use Junk on Local Folders  
  if (obj.settings.actionTargetFolder) 
    SetFolderPicker(obj.settings.actionTargetFolder, "actionTargetFolder");
  else {
    // note, this folder might not exist, but better this than ""
    // and, we'll create it if we try to use it.
    SetFolderPicker(kJunkOnLocalFolderURI, "actionTargetFolder");
  }

  // set up the purge UI
  document.getElementById("purge").checked = obj.settings.purge;
  document.getElementById("purgeInterval").value = obj.settings.purgeInterval;

  // set up the whitelist UI
  document.getElementById("useWhiteList").checked = obj.settings.useWhiteList;
  var abList = document.getElementById("whiteListAbURI");
  menuitems = abList.getElementsByAttribute("id", obj.settings.whiteListAbURI);
  abList.selectedItem = menuitems[0];

  // set up the manual mark UI
  document.getElementById("manualMark").checked = obj.settings.manualMark;
  document.getElementById("manualMarkMode").selectedItem = document.getElementById("manualMarkMode" + obj.settings.manualMarkMode);

  conditionallyEnableUI(null);
}

function junkLog()
{
  // pass in the "real" spam settings, as it's the one with the logStream
  var args = {spamSettings: gCurrentServer.spamSettings};
  window.openDialog("chrome://messenger/content/junkLog.xul", "junkLog", "chrome,modal,titlebar,resizable,centerscreen", args);
}

function onAccept()
{
  // store the current changes
  storeSettings(gSpamSettings[gCurrentServer.key].settings, gCurrentServer.spamSettings.loggingEnabled);

  for (var key in gSpamSettings) {
    try {
      // if they hit ok, set the "real" server's spam settings.  
      // this will set prefs.
      gSpamSettings[key].server.spamSettings = gSpamSettings[key].settings;
    }
    catch (ex) {
      dump("spam setting not saved: " + ex);
    }
  }

  // now store off our non account specific settings
  gPrefService.setBoolPref('mailnews.display.sanitizeJunkMail', document.getElementById('renderWithSafeHtml').checked);
  return true;
}

function storeSettings(aSettings, aLoggingEnabled)
{
  aSettings.level = document.getElementById("level").checked ? 100 : 0;
  aSettings.moveOnSpam = document.getElementById("moveOnSpam").checked;
  aSettings.moveTargetMode = document.getElementById("moveTargetMode").value;
  aSettings.actionTargetAccount = document.getElementById("actionTargetAccount").getAttribute("uri");
  var targetFolderURI = document.getElementById("actionTargetFolder").getAttribute("uri");
  if (targetFolderURI)
    aSettings.actionTargetFolder = targetFolderURI;
  else {
    // note, this folder might not exist, but better this than ""
    // and, we'll create it if we try to use it.
    aSettings.actionTargetFolder = kJunkOnLocalFolderURI;
  }

  aSettings.purge = document.getElementById("purge").checked;
  aSettings.purgeInterval = document.getElementById("purgeInterval").value;

  aSettings.useWhiteList = document.getElementById("useWhiteList").checked;
  aSettings.whiteListAbURI = document.getElementById("whiteListAbURI").selectedItem.getAttribute("id");
  aSettings.loggingEnabled = aLoggingEnabled;

  aSettings.manualMark = document.getElementById("manualMark").checked;
  aSettings.manualMarkMode = document.getElementById("manualMarkMode").value;
}

function conditionallyEnableUI(id)
{
  if (!document.getElementById("level").checked) {
    document.getElementById("useWhiteList").disabled = true;
    document.getElementById("whiteListAbURI").disabled = true;
    document.getElementById("moveOnSpam").disabled = true;

    document.getElementById("moveTargetMode").disabled = true;
    document.getElementById("actionTargetAccount").disabled = true;
    document.getElementById("actionTargetFolder").disabled = true;

    document.getElementById("purge").disabled = true;
    document.getElementById("purgeInterval").disabled = true;
    document.getElementById("purgeLabel").disabled = true;

    document.getElementById("manualMark").disabled = true;
    document.getElementById("manualMarkMode").disabled = true;
    return;
  }

  document.getElementById("useWhiteList").disabled = false;
  document.getElementById("moveOnSpam").disabled = false;
  document.getElementById("manualMark").disabled = false;

  var enabled;
  
  if (!id || id == "manualMark") {
    enabled = document.getElementById("manualMark").checked;
    // need to enable manualMarkMode before we enable manualMarkMode0
    document.getElementById("manualMarkMode").disabled = !enabled;
  }
 
  if (!id || id == "moveOnSpam") {
    enabled = document.getElementById("moveOnSpam").checked;
    var choice = document.getElementById("moveTargetMode").value;
 
    document.getElementById("moveTargetMode").disabled = !enabled;
    document.getElementById("actionTargetAccount").disabled = !enabled || (choice == 1);
    document.getElementById("actionTargetFolder").disabled = !enabled || (choice == 0);

    var checked = document.getElementById("purge").checked;
    document.getElementById("purge").disabled = !enabled;
    document.getElementById("purgeInterval").disabled = !enabled || !checked;
    document.getElementById("purgeLabel").disabled = !enabled;
    
    if (!document.getElementById("manualMarkMode").disabled)
      document.getElementById("manualMarkMode0").disabled = !enabled;
  }

  if (id == "purge") {
    enabled = document.getElementById("purge").checked;
    document.getElementById("purgeInterval").disabled = !enabled;
  }

  if (!id || id == "useWhiteList") {
    enabled = document.getElementById("useWhiteList").checked;
    document.getElementById("whiteListAbURI").disabled = !enabled;
  }
}

function doHelpButton()
{
  openHelp("mail-junk-controls");
}

function resetTrainingData()
{
  // make sure the user really wants to do this
  promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  var title = gMessengerBundle.getString("confirmResetJunkTrainingTitle");
  var text = gMessengerBundle.getString("confirmResetJunkTrainingText");

  // if the user says no, then just fall out
  if (!promptService.confirm(window, title, text))
    return;

  // otherwise go ahead and remove the training data
  var junkmailPlugin = Components.classes["@mozilla.org/messenger/filter-plugin;1?name=bayesianfilter"]
	                    .getService(Components.interfaces.nsIJunkMailPlugin);

  if (junkmailPlugin)
    junkmailPlugin.resetTrainingData();
}
