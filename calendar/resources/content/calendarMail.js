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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Dan Parent <danp@oeone.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var gMailAccounts = false;
var gMAILDEBUG = 2;  // 0 - no output, 1 dump to terminal, > 1 use alerts

/**** checkMail
 *
 * PURPOSE: if mailnews is installed return true, else false
 *
 */
 
function checkMail()
{
	var AccountManagerComponent;
	var AccountManagerService;
	var AccountManager;
	var DefaultAccount;
	var SmtpServiceComponent;
	var SmtpService;
	var Smtp;
	var DefaultSmtp;

	try
	{
		AccountManagerComponent = Components.classes["@mozilla.org/messenger/account-manager;1"];
		AccountManagerService = AccountManagerComponent.getService();
	}
	catch(ex) // yikes no account manager == no accounts
	{
		// from bug 122651 - scenario 1
		noMail();
		return;
	}
	try
	{
		AccountManager = AccountManagerService.QueryInterface(Components.interfaces.nsIMsgAccountManager);
		DefaultAccount = AccountManager.defaultAccount;
	}
	catch(ex)
	{
		// from bug 122651 - scenario 2
		noAccount();
		return;
	}
	try
	{
		SmtpServiceComponent = Components.classes["@mozilla.org/messengercompose/smtp;1"];
		SmtpService = SmtpServiceComponent.getService();
		Smtp = SmtpService.QueryInterface(Components.interfaces.nsISmtpService);
		DefaultSmtp = Smtp.defaultServer;
	}
	catch(ex)
	{
		// if we don't have an SMTP server to use I'm not going
		// to allow mail features to be enabled for now
		// from bug 122651: scenarios 3, and 6
		noSmtp();
		return;
	}
	// if we get here that means we must have a default account and default smtp server otherwise
	// the thrown exceptions would have been caught.
	// from bug 122651 - scenarios 4, 5, 7, and 8
	// we'll just use the default account for now.
	gMailAccounts = true;
}

/**** noMail
 *
 * PURPOSE: function to handle the scenario when mailnews is not installed
 *
 */
 
function noMail()
{
	// maybe provide a way of telling the user how to get a mailnews build
	// this should be a pref with a window that has a checkbox to shut off
	// this warning (for people that don't want mailnews installed)
	mdebug("You don't have mail installed");
}

/**** noAccount
 *
 * PURPOSE: function to handle the scenario where mailnews is installed but
 *          an account has not been created
 *
 */

function noAccount()
{
	// this could call the account wizard I guess thats a later feature
	mdebug("You don't have a mail account");
}


/**** noSmtp
 *
 * PURPOSE: function to handle the scenario where mailnews is installed, has
 *          a default incoming account but does not have an smtp service
 *
 * XXX: can this happen?
 *
 */

function noSmtp()
{
	/* I don't really know if its possible to create this scenario, I'm putting it here for
	 * the sake of completion so that I can say I've at least covered all possible scenarios
	 * from bug 122651
	 */
	 mdebug("You don't have an smtp account");
}

/**** mdebug
 *
 * PURPOSE: display debugging messages
 *
 */

function mdebug(message)
{
	if (gMAILDEBUG == 1)
	{
		dump(message + "\n");
	}
	else if (gMAILDEBUG > 1)
	{
		alert(message);
	}
}
