/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

var accountManagerProgID   = "component://netscape/messenger/account-manager";
var messengerMigratorProgID   = "component://netscape/messenger/migrator";

// returns the first account with an invalid server or identity

function getInvalidAccounts(accounts)
{
    var numAccounts = accounts.Count();
    var invalidAccounts = new Array;
    for (var i=0; i<numAccounts; i++) {
        var account = accounts.QueryElementAt(i, Components.interfaces.nsIMsgAccount);
        try {
            if (!account.incomingServer.valid) {
                dump("Found invalid server " + account.incomingServer + " in " + account + "!\n");
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
                dump("Found invalid identity " + identity + " in " + account + "\n");
                invalidAccounts[invalidAccounts.length] = account;
                continue;
            }
        }
    }

    dump("getInvalidAccounts found: \n");
    for (var i=0; i<invalidAccounts.length; i++) {
        
    }
    return invalidAccounts;
}

function verifyAccounts() {
    var openWizard = false;
    var prefillAccount;
    
    try {
        var am = Components.classes[accountManagerProgID].getService(Components.interfaces.nsIMsgAccountManager);

        var accounts = am.accounts;

        // as long as we have some accounts, we're fine.
        var accountCount = accounts.Count();
        var invalidAccounts = getInvalidAccounts(accounts);
        if (invalidAccounts.length > 0) {
            prefillAccount = invalidAccounts[0];
            dump("prefillAccount = " + prefillAccount + "\n");
        } else {
        }

        // if there are no accounts, or all accounts are "invalid"
        // then kick off the account migration
        if (accountCount == invalidAccounts.length) {
            try {
                messengerMigrator = Components.classes[messengerMigratorProgID].getService(Components.interfaces.nsIMessengerMigrator);  
                dump("attempt to UpgradePrefs.  If that fails, open the account wizard.\n");
                messengerMigrator.UpgradePrefs();
            }
            catch (ex) {
                // upgrade prefs failed, so open account wizard
                openWizard = true;
            }
        }

        if (openWizard || prefillAccount) {
            MsgAccountWizard(prefillAccount);
        }

    }
    catch (ex) {
        dump("error verifying accounts " + ex + "\n");
        return;
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
    window.openDialog("chrome://messenger/content/AccountWizard.xul",
                      "AccountWizard", "chrome,modal,titlebar,resizable");
}

function MsgAccountManager()
{
    var server;
    try {
        folderURI = GetSelectedFolderURI();
        server = GetServer(folderURI);
    } catch (ex) { /* functions might not be defined */}
    
    window.openDialog("chrome://messenger/content/AccountManager.xul",
                      "AccountManager", "chrome,modal,titlebar,resizable",
                      { server: server });
}
