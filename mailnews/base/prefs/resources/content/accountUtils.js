/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
// The returnmycall is used as a global variable that is set during a callback.
var returnmycall=false;
var accountManagerContractID   = "@mozilla.org/messenger/account-manager;1";
var messengerMigratorContractID   = "@mozilla.org/messenger/migrator;1";

// returns the first account with an invalid server or identity

function getInvalidAccounts(accounts)
{
    var numAccounts = accounts.Count();
    var invalidAccounts = new Array;
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
        var numIdentities = identities.Count();

        for (var j=0; j<numIdentities; j++) {
            var identity = identities.QueryElementAt(j, Components.interfaces.nsIMsgIdentity);
            if (!identity.valid) {
                invalidAccounts[invalidAccounts.length] = account;
                continue;
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
    var mapiRegistry;
    try {
        var mapiRegistryProgID = "@mozilla.org/mapiregistry;1" 
        if (mapiRegistryProgID in Components.classes) {
          mapiRegistry = Components.classes[mapiRegistryProgID].getService(Components.interfaces.nsIMapiRegistry);
        }
        else {
          mapiRegistry = null;
        }
    }
    catch (ex) { 
        mapiRegistry = null;
    }

    // showDialog is TRUE only if we did not bring up this dialog already
    // and we are not the default mail client
    var prefLocked = false;
    if (mapiRegistry && mapiRegistry.showDialog) {
        const prefbase = "system.windows.lock_ui.";
        try {
            prefService = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService()
                          .QueryInterface(Components.interfaces.nsIPrefService);
            prefBranch = prefService.getBranch(prefbase);
        
            if (prefBranch && prefBranch.prefIsLocked("defaultMailClient")) {
                prefLocked = true;
                if (prefBranch.getBoolPref("defaultMailClient"))
                    mapiRegistry.setDefaultMailClient();
                else
                    mapiRegistry.unsetDefaultMailClient();
            }
        }
        catch (ex) {}
        if (!prefLocked && !mapiRegistry.isDefaultMailClient)
            mapiRegistry.showMailIntegrationDialog();
    }
}

function verifyAccounts(wizardcallback) {
//check to see if the function is called with the callback and if so set the global variable returnmycall to true
    if(wizardcallback)
		returnmycall = true;
	var openWizard = false;
    var prefillAccount;
	var state=true;
	var ret = true;
    
    try {
        var am = Components.classes[accountManagerContractID].getService(Components.interfaces.nsIMsgAccountManager);

        var accounts = am.accounts;

        // as long as we have some accounts, we're fine.
        var accountCount = accounts.Count();
        var invalidAccounts = getInvalidAccounts(accounts);
        if (invalidAccounts.length > 0) {
            prefillAccount = invalidAccounts[0];
        } else {
        }

        // if there are no accounts, or all accounts are "invalid"
        // then kick off the account migration
        if (accountCount == invalidAccounts.length) {
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

        if (openWizard || prefillAccount) {
            MsgAccountWizard(prefillAccount);
		        ret = false;
        }
        showMailIntegrationDialog();
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
// Check to see if the verify accounts function was called with callback or not.
  if (returnmycall)
      window.openDialog("chrome://messenger/content/AccountWizard.xul",
                        "AccountWizard", "chrome,modal,titlebar,resizable", {okCallback:WizCallback});
  else
      window.openDialog("chrome://messenger/content/AccountWizard.xul",
                        "AccountWizard", "chrome,modal,titlebar,resizable");

}

// selectPage: the xul file name for the viewing page, 
// null for the account main page, other pages are
// 'am-server.xul', 'am-copies.xul', 'am-offline.xul', 
// 'am-addressing.xul','am-advanced.xul', 'am-smtp.xul'
function MsgAccountManager(selectPage)
{
    var server;
    try {
        var folderURI = GetSelectedFolderURI();
        server = GetServer(folderURI);
    } catch (ex) { /* functions might not be defined */}
    
    window.openDialog("chrome://messenger/content/AccountManager.xul",
                      "AccountManager", "chrome,modal,titlebar,resizable",
                      { server: server, selectPage: selectPage });
}
