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
 * The Original Code is mozilla.org code.
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
// The gReturnmycall is used as a global variable that is set during a callback.
var gReturnmycall=false;
var accountManagerContractID   = "@mozilla.org/messenger/account-manager;1";
var messengerMigratorContractID   = "@mozilla.org/messenger/migrator;1";
var gAnyValidIdentity = false; //If there are no valid identities for any account
// returns the first account with an invalid server or identity

var gNewAccountToLoad = null;   // used to load new messages if we come from the mail3pane

function getInvalidAccounts(accounts)
{
    var numAccounts = accounts.Count();
    var invalidAccounts = new Array;
    var numIdentities = 0;
    for (var i=0; i<numAccounts; i++) {
        var account = accounts.QueryElementAt(i, Components.interfaces.nsIMsgAccount);
        try {
            if (!account.incomingServer.valid) {
                invalidAccounts[invalidAccounts.length] = account;
                // skip to the next account
                continue;
            }
        } catch (ex) {
            // this account is busted, just keep going
            continue;
        }

        var identities = account.identities;
        numIdentities = identities.Count();

        for (var j=0; j<numIdentities; j++) {
            var identity = identities.QueryElementAt(j, Components.interfaces.nsIMsgIdentity);
            if (identity.valid) {
              gAnyValidIdentity = true;
            }
            else {
              invalidAccounts[invalidAccounts.length] = account;
            }
        }
    }
    return invalidAccounts;
}

// This function gets called from verifyAccounts.
// We do not have to do anything on
// unix and mac but on windows we have to bring up a 
// dialog based on the settings the user has.
// This will bring up the dialog only once per session and only if we
// are not the default mail client.
function showMailIntegrationDialog() {
    var mapiRegistry = null;
    try {
        var mapiRegistryProgID = "@mozilla.org/mapiregistry;1" 
        if (mapiRegistryProgID in Components.classes) {
          mapiRegistry = Components.classes[mapiRegistryProgID].getService(Components.interfaces.nsIMapiRegistry);
        }
    }
    catch (ex) { 
    }

    // showDialog is TRUE only if we did not bring up this dialog already
    // and we are not the default mail client
    var prefLocked = false;
    if (mapiRegistry && mapiRegistry.showDialog) {
        const prefbase = "system.windows.lock_ui.";
        try {
            var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService()
                          .QueryInterface(Components.interfaces.nsIPrefService);
            var prefBranch = prefService.getBranch(prefbase);
        
            if (prefBranch && prefBranch.prefIsLocked("defaultMailClient")) {
                prefLocked = true;
                mapiRegistry.isDefaultMailClient = prefBranch.getBoolPref("defaultMailClient") ;
            }
        }
        catch (ex) {}
        try {
          if (!prefLocked)
            mapiRegistry.showMailIntegrationDialog(window /* parent window */);
        }
        catch (ex) {
          dump("mapi code failed:  " + ex + "\n");
        }
    }
}

function verifyAccounts(wizardcallback) 
{
  //check to see if the function is called with the callback and if so set the global variable gReturnmycall to true
  if(wizardcallback)
		gReturnmycall = true;
	var openWizard = false;
  var prefillAccount;
	var state=true;
	var ret = true;
    
    try {
        var am = Components.classes[accountManagerContractID].getService(Components.interfaces.nsIMsgAccountManager);

        // migrate quoting preferences from global to per account. This function returns
        // true if it had to migrate, which we will use to mean this is a just migrated
        // or new profile
        var newProfile = migrateGlobalQuotingPrefs(am.allIdentities);

        var accounts = am.accounts;

        // as long as we have some accounts, we're fine.
        var accountCount = accounts.Count();
        var invalidAccounts = getInvalidAccounts(accounts);
        if (invalidAccounts.length > 0) {
            prefillAccount = invalidAccounts[0];
        } else {
        }

        // if there are no accounts, or all accounts are "invalid"
        // then kick off the account migration. Or if this is a new (to Mozilla) profile.
        // MCD can set up accounts without the profile being used yet
        if (newProfile) {
          // check if MCD is configured. If not, say this is not a new profile
          // so that we don't accidentally remigrate non MCD profiles.
          var adminUrl;
          var pref = Components.classes["@mozilla.org/preferences-service;1"]
                                 .getService(Components.interfaces.nsIPrefBranch);
          try {
            adminUrl = pref.getCharPref("autoadmin.global_config_url");
          }
          catch (ex) {}
          if (!adminUrl)
            newProfile = false;
        }
        if ((newProfile  && !accountCount) || accountCount == invalidAccounts.length) {
            try {
                  var messengerMigrator = Components.classes[messengerMigratorContractID].getService(Components.interfaces.nsIMessengerMigrator); 
                  messengerMigrator.UpgradePrefs();
                  // if there is a callback mechanism then inform parent window to shut itself down
                  if (wizardcallback){
                      state = false;
                      WizCallback(state);
                  }
                  ret = false;
            }
            catch (ex) {
                  // upgrade prefs failed, so open account wizard
                  openWizard = true;
            }
        }

        //We are doing openWizard if  MessengerMigration returns some kind of error
        //(including those cases where there is nothing to migrate).
        //prefillAccount is valid, if there is an invalid account already 
        //gAnyValidIdentity is true when you've got at least one *valid* identity,
        //Since local folders is an identity-less account, if you only have
        //local folders, it will be false.
        //wizardcallback is true only when verifyaccounts is called from compose window.
        //the last condition in the if is so that we call account wizard only when the user
        //has only a local folder and tries to compose mail.

        if (openWizard || prefillAccount || ((!gAnyValidIdentity) && wizardcallback)) {
            MsgAccountWizard();
		        ret = false;
        }
        else
        {
          var localFoldersExists;
          try
          {
            localFoldersExists = am.localFoldersServer;
          }
          catch (ex)
          {
            localFoldersExists = false;
          }

          // we didn't create the MsgAccountWizard - we need to verify that local folders exists.
          if (!localFoldersExists)
          {
            messengerMigrator = Components.classes["@mozilla.org/messenger/migrator;1"].getService(Components.interfaces.nsIMessengerMigrator);
            messengerMigrator.createLocalMailAccount(false /* false, since we are not migrating */);
          }
        }
        
        // This will only succeed on SeaMonkey windows builds
        var mapiRegistryProgID = "@mozilla.org/mapiregistry;1" 
        if (mapiRegistryProgID in Components.classes)
        {
          // hack, set a time out to do this, so that the window can load first
          setTimeout(showMailIntegrationDialog, 0);
        }

        return ret;
    }
    catch (ex) {
        dump("error verifying accounts " + ex + "\n");
        return false;
    }
}

// we do this from a timer because if this is called from the onload=
// handler, then the parent window doesn't appear until after the wizard
// has closed, and this is confusing to the user
function MsgAccountWizard()
{
  setTimeout("msgOpenAccountWizard();", 0);
}

function msgOpenAccountWizard()
{
  gNewAccountToLoad = null;

  // Check to see if the verify accounts function 
  // was called with callback or not.
  if (gReturnmycall)
      window.openDialog("chrome://messenger/content/AccountWizard.xul",
                        "AccountWizard", "chrome,modal,titlebar,centerscreen", {okCallback:WizCallback});
  else
      window.openDialog("chrome://messenger/content/AccountWizard.xul",
                        "AccountWizard", "chrome,modal,titlebar,centerscreen");

  loadInboxForNewAccount();

  //For the first account we need to reset the default smtp server in the panel.
  var smtpService = Components.classes["@mozilla.org/messengercompose/smtp;1"].getService(Components.interfaces.nsISmtpService);
  var serverCount = smtpService.smtpServers.Count();
  try{ReloadSmtpPanel();}
  catch(ex){}
}

// selectPage: the xul file name for the viewing page, 
// null for the account main page, other pages are
// 'am-server.xul', 'am-copies.xul', 'am-offline.xul', 
// 'am-addressing.xul', 'am-smtp.xul'
function MsgAccountManager(selectPage)
{
    var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].
                            getService(Components.interfaces.nsIWindowMediator);

    var existingAccountManager = windowManager.getMostRecentWindow("mailnews:accountmanager");

    if (existingAccountManager)
        existingAccountManager.focus();
    else {
        var server;
        try {
            var folderURI = GetSelectedFolderURI();
            server = GetServer(folderURI);
        } catch (ex) { /* functions might not be defined */}
        
        window.openDialog("chrome://messenger/content/AccountManager.xul",
                          "AccountManager", "chrome,centerscreen,modal,titlebar",
                          { server: server, selectPage: selectPage });
    }
}

function loadInboxForNewAccount() 
{
  // gNewAccountToLoad is set in the final screen of the Account Wizard if a POP account
  // was created, the download messages box is checked, and the wizard was opened from the 3pane
  if (gNewAccountToLoad) {
    var rootMsgFolder = gNewAccountToLoad.incomingServer.rootMsgFolder;
    var outNumFolders = new Object();
    var inboxFolder = rootMsgFolder.getFoldersWithFlag(0x1000, 1, outNumFolders);
    SelectFolder(inboxFolder.URI);
    window.focus();
    setTimeout(MsgGetMessage, 0);
    gNewAccountToLoad = null;
  }
}

// returns true if we migrated - it knows this because 4.x did not have the
// pref mailnews.quotingPrefs.version, so if it's not set, we're either 
// migrating from 4.x, or a much older version of Mozilla.
function migrateGlobalQuotingPrefs(allIdentities)
{
  // if reply_on_top and auto_quote exist then, if non-default
  // migrate and delete, if default just delete.  
  var reply_on_top = 0;
  var auto_quote = true;
  var quotingPrefs = 0;
  var migrated = false;
  try {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefService);
    var pref = prefService.getBranch(null);
    quotingPrefs = pref.getIntPref("mailnews.quotingPrefs.version");
  } catch (ex) {}
  
  // If the quotingPrefs version is 0 then we need to migrate our preferences
  if (quotingPrefs == 0) {
    migrated = true;
    try {
      reply_on_top = pref.getIntPref("mailnews.reply_on_top");
      auto_quote = pref.getBoolPref("mail.auto_quote");
    } catch (ex) {}

    if (!auto_quote || reply_on_top) {
      var numIdentities = allIdentities.Count();
      var identity = null;
      for (var j = 0; j < numIdentities; j++) {
        identity = allIdentities.QueryElementAt(j, Components.interfaces.nsIMsgIdentity);
        if (identity.valid) {
          identity.autoQuote = auto_quote;
          identity.replyOnTop = reply_on_top;
        }
      }
    }
    pref.setIntPref("mailnews.quotingPrefs.version", 1);
  }
  return migrated;
}
