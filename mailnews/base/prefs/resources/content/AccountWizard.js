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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* the okCallback is used for sending a callback for the parent window */
var okCallback = null;
/* The account wizard creates new accounts */

/*
  data flow into the account wizard like this:

  For new accounts:
  * pageData -> Array -> createAccount -> finishAccount
  
  For accounts coming from the ISP setup:
  * RDF  -> Array -> pageData -> Array -> createAccount -> finishAccount
  
  for "unfinished accounts" 
  * account -> Array -> pageData -> Array -> finishAccount
  
  Where:
  pageData - the actual pages coming out of the Widget State Manager
  RDF      - the ISP datasource
  Array    - associative array of attributes, that very closely
             resembles the nsIMsgAccount/nsIMsgIncomingServer/nsIMsgIdentity
             structure
  createAccount() - creates an account from the above Array
  finishAccount() - fills an existing account with data from the above Array 

*/

/* 
   the account wizard path is something like:
   
   accounttype -> identity -> server -> login -> accname -> done
                             \-> newsserver ----/

   where the accounttype determines which path to take
   (server vs. newsserver)
*/

var contentWindow;

var gPageData;
var smtpService = Components.classes["@mozilla.org/messengercompose/smtp;1"].getService(Components.interfaces.nsISmtpService);
var am = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
var gPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
var gMailSession = Components.classes["@mozilla.org/messenger/services/session;1"].getService(Components.interfaces.nsIMsgMailSession);
var accounts = am.accounts;

//accountCount indicates presence or absense of accounts
var accountCount = accounts.Count();

var nsIMsgIdentity = Components.interfaces.nsIMsgIdentity;
var nsIMsgIncomingServer = Components.interfaces.nsIMsgIncomingServer;
var gPrefsBundle, gMessengerBundle;
var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);

// the current nsIMsgAccount
var gCurrentAccount;

// default account
var gDefaultAccount;

// the current associative array that
// will eventually be dumped into the account
var gCurrentAccountData;

// default picker mode for copies and folders
const gDefaultSpecialFolderPickerMode = "0";

// event handlers
function onAccountWizardLoad() {
    gPrefsBundle = document.getElementById("bundle_prefs");
    gMessengerBundle = document.getElementById("bundle_messenger");

    if ("testingIspServices" in this) {
      if ("SetCustomizedWizardDimensions" in this && testingIspServices()) {
        SetCustomizedWizardDimensions();
      }
    }

    /* We are checking here for the callback argument */
    if (window.arguments && window.arguments[0]) {
        if(window.arguments[0].okCallback ) 
        {
            //dump("There is okCallback");
            top.okCallback = window.arguments[0].okCallback;
        }
    }

    checkForInvalidAccounts();

    if (gCurrentAccount) {
        // Set the page index to identity page.
        document.documentElement.pageIndex = 1;
    }

    try {
        gDefaultAccount = am.defaultAccount;
    }
    catch (ex) {
        // no default account, this is expected the first time you launch mail
        // on a new profile
        gDefaultAccount = null;
    }
}

function onCancel() 
{
  if ("ActivationOnCancel" in this && ActivationOnCancel())
    return false;
  var firstInvalidAccount = getFirstInvalidAccount();
  var closeWizard = true;

  // if the user cancels the the wizard when it pops up because of 
  // an invalid account (example, a webmail account that activation started)
  // we just force create it by setting some values and calling the FinishAccount()
  // see bug #47521 for the full discussion
  if (firstInvalidAccount) {
    var pageData = GetPageData();
    // set the fullName if it doesn't exist
    if (!pageData.identity.fullName || !pageData.identity.fullName.value) {
      setPageData(pageData, "identity", "fullName", "");
    }

    // set the email if it doesn't exist
    if (!pageData.identity.email || !pageData.identity.email.value) {
      setPageData(pageData, "identity", "email", "user@domain.invalid");
    }

    // call FinishAccount() and not onFinish(), since the "finish"
    // button may be disabled
    FinishAccount();
  }
  else {
    // since this is not an invalid account
    // really cancel if the user hits the "cancel" button
    if (accountCount < 1) {
      var confirmMsg = gPrefsBundle.getString("cancelWizard");
      var confirmTitle = gPrefsBundle.getString("accountWizard");
      var result = promptService.confirmEx(window, confirmTitle, confirmMsg,
        (promptService.BUTTON_TITLE_IS_STRING*promptService.BUTTON_POS_0)+
        (promptService.BUTTON_TITLE_IS_STRING*promptService.BUTTON_POS_1),
        gPrefsBundle.getString('WizardExit'),
        gPrefsBundle.getString('WizardContinue'), 
        null, null, {value:0});

      if (result == 1)
        closeWizard = false;
    }

    if(top.okCallback && closeWizard) {
      var state = false;
      top.okCallback(state);
    }
  }
  return closeWizard;
}

function FinishAccount() 
{
  try {
    var pageData = GetPageData();

    var accountData= gCurrentAccountData;
    
    if (!accountData)
    {
      accountData = new Object;
      // Time to set the smtpRequiresUsername attribute
      if (!serverIsNntp(pageData))
        accountData.smtpRequiresUsername = true;
    }
    
    // we may need local folders before account is "Finished"
    // if it's a pop3 account which defers to Local Folders.
    verifyLocalFoldersAccount();

    PageDataToAccountData(pageData, accountData);

    FixupAccountDataForIsp(accountData);
    
    // we might be simply finishing another account
    if (!gCurrentAccount)
        gCurrentAccount = createAccount(accountData);

    // transfer all attributes from the accountdata
    finishAccount(gCurrentAccount, accountData);
    
    setupCopiesAndFoldersServer(gCurrentAccount, getCurrentServerIsDeferred(pageData));

    if (!serverIsNntp(pageData))
        EnableCheckMailAtStartUpIfNeeded(gCurrentAccount);

    if (!document.getElementById("downloadMsgs").hidden) {
      if (document.getElementById("downloadMsgs").checked) {
        window.opener.gNewAccountToLoad = gCurrentAccount; // load messages for new POP account
      }
      else if (gCurrentAccount == am.defaultAccount) {
        // stop check for msgs when this is first account created from new profile
        window.opener.gLoadStartFolder = false;
      }
    }

    // in case we crash, force us a save of the prefs file NOW
    try {
      am.saveAccountInfo();
    } 
    catch (ex) {
      dump("Error saving account info: " + ex + "\n");
    }
    window.close();
    if(top.okCallback)
    {
        var state = true;
        //dump("finish callback");
        top.okCallback(state);
    }
}
  catch(ex) {
    dump("FinishAccount failed, " + ex +"\n");
  }
}

// prepopulate pageData with stuff from accountData
// use: to prepopulate the wizard with account information
function AccountDataToPageData(accountData, pageData)
{
    if (!accountData) {
        dump("null account data! clearing..\n");
        // handle null accountData as if it were an empty object
        // so that we clear-out any old pagedata from a
        // previous accountdata. The trick is that
        // with an empty object, accountData.identity.slot is undefined,
        // so this will clear out the prefill data in setPageData
        
        accountData = new Object;
        accountData.incomingServer = new Object;
        accountData.identity = new Object;
        accountData.smtp = new Object;
    }
    
    var server = accountData.incomingServer;

    if (server.type == undefined) {
        // clear out the old server data
        //setPageData(pageData, "accounttype", "mailaccount", undefined);
        //        setPageData(pageData, "accounttype", "newsaccount", undefined);
        setPageData(pageData, "server", "servertype", undefined);
        setPageData(pageData, "server", "hostname", undefined);
        
    } else {
        
        if (server.type == "nntp") {
            setPageData(pageData, "accounttype", "newsaccount", true);
            setPageData(pageData, "accounttype", "mailaccount", false);
            setPageData(pageData, "newsserver", "hostname", server.hostName);
        }
        
        else {
            setPageData(pageData, "accounttype", "mailaccount", true);
            setPageData(pageData, "accounttype", "newsaccount", false);
            setPageData(pageData, "server", "servertype", server.type);
            setPageData(pageData, "server", "hostname", server.hostName);
        }
    }
    
    setPageData(pageData, "login", "username", server.username);
    setPageData(pageData, "login", "password", server.password);
    setPageData(pageData, "login", "rememberPassword", server.rememberPassword);
    setPageData(pageData, "accname", "prettyName", server.prettyName);
    
    var identity;
    
    if (accountData.identity) {
        dump("This is an accountdata\n");
        identity = accountData.identity;
    }
    else if (accountData.identities) {
        identity = accountData.identities.QueryElementAt(0, Components.interfaces.nsIMsgIdentity);
        dump("this is an account, id= " + identity + "\n");
    } 

    setPageData(pageData, "identity", "email", identity.email);
    setPageData(pageData, "identity", "fullName", identity.fullName);

    var smtp;
    
    if (accountData.smtp) {
        smtp = accountData.smtp;
        setPageData(pageData, "server", "smtphostname", smtp.hostname);
        setPageData(pageData, "login", "smtpusername", smtp.username);
    }
}

// take data from each page of pageData and dump it into accountData
// use: to put results of wizard into a account-oriented object
function PageDataToAccountData(pageData, accountData)
{
    if (!accountData.identity)
        accountData.identity = new Object;
    if (!accountData.incomingServer)
        accountData.incomingServer = new Object;
    if (!accountData.smtp)
        accountData.smtp = new Object;
    if (!accountData.pop3)
        accountData.pop3 = new Object;
    
    var identity = accountData.identity;
    var server = accountData.incomingServer;
    var smtp = accountData.smtp;

    if (pageData.identity.email)
        identity.email = pageData.identity.email.value;
    if (pageData.identity.fullName)
        identity.fullName = pageData.identity.fullName.value;

    server.type = getCurrentServerType(pageData);
    server.hostName = getCurrentHostname(pageData);
    if (getCurrentServerIsDeferred(pageData))
    {
      try
      {
        var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
        var localFoldersServer = accountManager.localFoldersServer;
        var localFoldersAccount = accountManager.FindAccountForServer(localFoldersServer);
        accountData.pop3.deferredToAccount = localFoldersAccount.key;
        accountData.pop3.deferGetNewMail = true;
        server["ServerType-pop3"] = accountData.pop3;
      }
      catch (ex) {dump ("exception setting up deferred account" + ex);}
    }
    if (serverIsNntp(pageData)) {
        // this stuff probably not relevant
        dump("not setting username/password/rememberpassword/etc\n");
    } else {
        if (pageData.login) {
            if (pageData.login.username)
                server.username = pageData.login.username.value;
            if (pageData.login.password)
                server.password = pageData.login.password.value;
            if (pageData.login.rememberPassword)
                server.rememberPassword = pageData.login.rememberPassword.value;
            if (pageData.login.smtpusername)
                smtp.username = pageData.login.smtpusername.value;
        }

        dump("pageData.server = " + pageData.server + "\n");
        if (pageData.server) {
            dump("pageData.server.smtphostname.value = " + pageData.server.smtphostname + "\n");
            if (pageData.server.smtphostname &&
                pageData.server.smtphostname.value)
                smtp.hostname = pageData.server.smtphostname.value;
        }
        if (pageData.identity.smtpServerKey)
            identity.smtpServerKey = pageData.identity.smtpServerKey.value;
    }

    if (pageData.accname) {
        if (pageData.accname.prettyName)
            server.prettyName = pageData.accname.prettyName.value;
    }

}

// given an accountData structure, create an account
// (but don't fill in any fields, that's for finishAccount()
function createAccount(accountData)
{
    var server = accountData.incomingServer;
    
    // for news, username is always null
    var username = (server.type == "nntp") ? null : server.username;

    dump("am.createIncomingServer(" + username + "," +
                                      server.hostName + "," +
                                      server.type + ")\n");
    
    var server = am.createIncomingServer(username,
                                         server.hostName,
                                         server.type);

    dump("am.createAccount()\n");
    var account = am.createAccount();
    
    if (accountData.identity.email) // only create an identity for this account if we really have one (use the email address as a check)
    {
        dump("am.createIdentity()\n");
        var identity = am.createIdentity();
    
        /* new nntp identities should use plain text by default
         * we want that GNKSA (The Good Net-Keeping Seal of Approval) */
        if (server.type == "nntp") {
			    identity.composeHtml = false;
        }

        account.addIdentity(identity);
    }

    // we mark the server as invalid so that the account manager won't
    // tell RDF about the new server - it's not quite finished getting
    // set up yet, in particular, the deferred storage pref hasn't been set.
    server.valid = false;
    account.incomingServer = server;
    server.valid = true;
    return account;
}

// given an accountData structure, copy the data into the
// given account, incoming server, and so forth
function finishAccount(account, accountData) 
{
    if (accountData.incomingServer) {

        var destServer = account.incomingServer;
        var srcServer = accountData.incomingServer;
        copyObjectToInterface(destServer, srcServer);

        // see if there are any protocol-specific attributes
        // if so, we use the type to get the IID, QueryInterface
        // as appropriate, then copy the data over
        dump("srcServer.ServerType-" + srcServer.type + " = " +
             srcServer["ServerType-" + srcServer.type] + "\n");
        if (srcServer["ServerType-" + srcServer.type]) {
            // handle server-specific stuff
            var IID = getInterfaceForType(srcServer.type);
            if (IID) {
                destProtocolServer = destServer.QueryInterface(IID);
                srcProtocolServer = srcServer["ServerType-" + srcServer.type];

                dump("Copying over " + srcServer.type + "-specific data\n");
                copyObjectToInterface(destProtocolServer, srcProtocolServer);
            }
        }
        account.incomingServer.valid=true;
        // hack to cause an account loaded notification now the server is valid
        account.incomingServer = account.incomingServer;
    }

    // copy identity info
    var destIdentity = account.identities.Count() ? account.identities.QueryElementAt(0, nsIMsgIdentity) : null;

    if (destIdentity) // does this account have an identity? 
    {   
        if (accountData.identity && accountData.identity.email) {
            dump('trying to write out an identity: ' + destIdentity + '\n');

            // fixup the email address if we have a default domain
            var emailArray = accountData.identity.email.split('@');
            if (emailArray.length < 2 && accountData.domain) {
                accountData.identity.email += '@' + accountData.domain;
            }

            copyObjectToInterface(destIdentity,
                                  accountData.identity);
            destIdentity.valid=true;
        }

        /**
         * If signature file need to be set, get the path to the signature file.
         * Signature files, if exist, are placed under default location. Get
         * default files location for messenger using directory service. Signature 
         * file name should be extracted from the account data to build the complete
         * path for signature file. Once the path is built, set the identity's signature pref.
         */
        if (destIdentity.attachSignature)
        {
            var sigFileName = accountData.signatureFileName;
      
            var sigFile = gMailSession.getDataFilesDir("messenger");
            sigFile.append(sigFileName);
            destIdentity.signature = sigFile;
        }

        // don't try to create an smtp server if we already have one.
        if (!destIdentity.smtpServerKey)
        {
            var smtpServer;
        
            /**
             * Create a new smtp server if needed. If smtpCreateNewServer pref
             * is set then createSmtpServer routine() will create one. Otherwise,
             * default server is returned which is also set to create a new smtp server
             * (via GetDefaultServer()) if no default server is found.
             */
            if (accountData.smtp.hostname != null)
              smtpServer = smtpService.createSmtpServer();
            else
              smtpServer = smtpService.defaultServer;

            // may not have a smtp server, see bug #138076
            if (smtpServer) {
              dump("Copying smtpServer (" + smtpServer + ") to accountData\n");
              //set the smtp server to be the default only if it is not a redirectorType
              if (accountData.smtp.redirectorType == null) 
              {
                if ((smtpService.defaultServer.hostname == null) || (smtpService.defaultServer.redirectorType != null))
                  smtpService.defaultServer = smtpServer;
              }

              copyObjectToInterface(smtpServer, accountData.smtp);

              // refer bug#141314
              // since we clone the default smtpserver with the new account's username
              // force every account to use the smtp server that was created or assigned to it in the
              // case of isps using rdf files
              try{
                destIdentity.smtpServerKey = smtpServer.key;
              }
              catch(ex)
              {
                dump("There is no smtp server assigned to this account: Exception= "+ex+"\n");
              }
            }
         }
     } // if the account has an identity...

     if (this.FinishAccountHook != undefined) {
         FinishAccountHook(accountData.domain);
     }
}

// copy over all attributes from dest into src that already exist in src
// the assumption is that src is an XPConnect interface full of attributes
function copyObjectToInterface(dest, src) {
    if (!dest) return;
    if (!src) return;

    var i;
    for (i in src) {
        try {
            if (dest[i] != src[i])
                dest[i] = src[i];
        }
        catch (ex) {
            dump("Error copying the " +
                 i + " attribute: " + ex + "\n");
            dump("[This is ok if this is a ServerType-* attribute, or if this is a readonly attribute (like receiptHeaderType)]\n");
        }
    }
}

// check if there already is a "Local Folders"
// if not, create it.
function verifyLocalFoldersAccount() 
{
  dump ("verifying local folders account\n");
  var localMailServer = null;
  try {
    localMailServer = am.localFoldersServer;
  }
  catch (ex) {
         // dump("exception in findserver: " + ex + "\n");
    localMailServer = null;
  }

  try {

    if (!localMailServer) 
    {
      // dump("Creating local mail account\n");
      // creates a copy of the identity you pass in
      messengerMigrator = Components.classes["@mozilla.org/messenger/migrator;1"].getService(Components.interfaces.nsIMessengerMigrator);
      messengerMigrator.createLocalMailAccount(false /* false, since we are not migrating */);
      try {
        localMailServer = am.localFoldersServer;
      }
      catch (ex) {
        dump("error!  we should have found the local mail server after we created it.\n");
        localMailServer = null;
      }	
    }
  }
  catch (ex) {dump("Error in verifyLocalFoldersAccount" + ex + "\n");  }

}

function setupCopiesAndFoldersServer(account, accountIsDeferred)
{
  try {
    var server = account.incomingServer;
    var identity = account.identities.QueryElementAt(0, Components.interfaces.nsIMsgIdentity);
    // For this server, do we default the folder prefs to this server, or to the "Local Folders" server
    // If it's deferred, we use the local folders account.
    var defaultCopiesAndFoldersPrefsToServer = !accountIsDeferred && server.defaultCopiesAndFoldersPrefsToServer;
    dump ("verifying local folders account \n");

    var copiesAndFoldersServer = null;
    if (defaultCopiesAndFoldersPrefsToServer) 
    {
      copiesAndFoldersServer = server;
    }
    else 
    {
      if (!am.localFoldersServer) 
      {
        dump("error!  we should have a local mail server at this point\n");
        return;
      }
      copiesAndFoldersServer = am.localFoldersServer;
    }
	  
    setDefaultCopiesAndFoldersPrefs(identity, copiesAndFoldersServer);

  } catch (ex) {
    // return false (meaning we did not setupCopiesAndFoldersServer)
    // on any error
    dump("Error setupCopiesAndFoldersServer" + ex + "\n");
    return false;
  }
  return true;
}

function setDefaultCopiesAndFoldersPrefs(identity, server)
{
	dump("finding folders on server = " + server.hostName + "\n");

	var rootFolder = server.rootFolder;

	// we need to do this or it is possible that the server's draft,
    // stationery fcc folder will not be in rdf
	//
	// this can happen in a couple cases
	// 1) the first account we create, creates the local mail.  since
    // local mail was just created, it obviously hasn't been opened,
    // or in rdf..
	// 2) the account we created is of a type where
    // defaultCopiesAndFoldersPrefsToServer is true
	// this since we are creating the server, it obviously hasn't been
    // opened, or in rdf.
	//
	// this makes the assumption that the server's draft, stationery fcc folder
	// are at the top level (ie subfolders of the root folder.)  this works
    // because we happen to be doing things that way, and if the user changes
    // that, it will work because to change the folder, it must be in rdf,
    // coming from the folder cache, in the worst case.
	var folders = rootFolder.GetSubFolders();
	var msgFolder = rootFolder.QueryInterface(Components.interfaces.nsIMsgFolder);
	var numFolders = new Object();

	var protocolInfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + msgFolder.server.type].getService(Components.interfaces.nsIMsgProtocolInfo);

    /** 
     * Check if this protocol service needs to create special folder URIs.
     * In case of IMAP, when a new account is created, folders 'Sent', 'Drafts'
     * and 'Templates' are not created then, but created on demand at runtime. 
     * But we do need to present them as possible choices in the Copies and Folders 
     * UI. To do that, folder URIs have to be created and stored in the prefs file. 
     * So, if there is a need to build special folders, append the special folder 
     * names and create right URIs.
     */
    if (protocolInfo.needToBuildSpecialFolderURIs)
    {
        var folderDelim = "/";

        /* we use internal names known to everyone like Sent, Templates and Drafts */

        identity.draftFolder = msgFolder.server.serverURI+ folderDelim + "Drafts";
        identity.stationeryFolder = msgFolder.server.serverURI+ folderDelim + "Templates";
        identity.fccFolder = msgFolder.server.serverURI+ folderDelim + "Sent";
    }
    else {
        // these hex values come from nsMsgFolderFlags.h
	var draftFolder = msgFolder.getFoldersWithFlag(0x0400, 1, numFolders);
	var stationeryFolder = msgFolder.getFoldersWithFlag(0x400000, 1, numFolders);
	var fccFolder = msgFolder.getFoldersWithFlag(0x0200, 1, numFolders);

	if (draftFolder) identity.draftFolder = draftFolder.URI;
	if (stationeryFolder) identity.stationeryFolder = stationeryFolder.URI;
	if (fccFolder) identity.fccFolder = fccFolder.URI;

	dump("fccFolder = " + identity.fccFolder + "\n");
	dump("draftFolder = " + identity.draftFolder + "\n");
	dump("stationeryFolder = " + identity.stationeryFolder + "\n");
    }
	
    identity.fccFolderPickerMode = gDefaultSpecialFolderPickerMode;
    identity.draftsFolderPickerMode = gDefaultSpecialFolderPickerMode;
    identity.tmplFolderPickerMode = gDefaultSpecialFolderPickerMode;
}

function AccountExists(userName,hostName,serverType)
{
  var accountExists = false;
  var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
  try {
        var server = accountManager.findRealServer(userName,hostName,serverType);
        if (server) {
                accountExists = true;
        }
  }
  catch (ex) {
        accountExists = false;
  }
  return accountExists;
}

function getFirstInvalidAccount()
{
    am = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);

    var invalidAccounts = getInvalidAccounts(am.accounts);
  
    if (invalidAccounts.length > 0)
        return invalidAccounts[0];
    else
        return null;
}

function checkForInvalidAccounts()
{
	var firstInvalidAccount = getFirstInvalidAccount();

    if (firstInvalidAccount) {
        var pageData = GetPageData();
        dump("We have an invalid account, " + firstInvalidAccount + ", let's use that!\n");
        gCurrentAccount = firstInvalidAccount;

        // there's a possibility that the invalid account has ISP defaults
        // as well.. so first pre-fill accountData with ISP info, then
        // overwrite it with the account data


        var identity =
            firstInvalidAccount.identities.QueryElementAt(0, nsIMsgIdentity);

        var accountData = null;
        // If there is a email address already provided, try to get to other ISP defaults.
        // If not, get pre-configured data, if any.
        if (identity.email) {
          dump("Invalid account: trying to get ISP data for " + identity.email + "\n");
          accountData = getIspDefaultsForEmail(identity.email);
          dump("Invalid account: Got " + accountData + "\n");

          // account -> accountData -> pageData
          accountData = AccountToAccountData(firstInvalidAccount, accountData);
        }
        else {
          accountData = getPreConfigDataForAccount(firstInvalidAccount);
        }
        
        AccountDataToPageData(accountData, pageData);

        gCurrentAccountData = accountData;
    }
}

// Transfer all invalid account information to AccountData. Also, get those special 
// preferences (not associated with any interfaces but preconfigurable via prefs or rdf files) 
// like whether not the smtp server associated with this account requires 
// a user name (mail.identity.<id_key>.smtpRequiresUsername) and the choice of skipping 
// panels (mail.identity.<id_key>.wizardSkipPanels).
function getPreConfigDataForAccount(account)
{
  var accountData = new Object;
  accountData = new Object;
  accountData.incomingServer = new Object;
  accountData.identity = new Object;
  accountData.smtp = new Object;

  accountData = AccountToAccountData(account, null);

  var identity = account.identities.QueryElementAt(0, nsIMsgIdentity);

  try {
    var skipPanelsPrefStr = "mail.identity." + identity.key + ".wizardSkipPanels";
    accountData.wizardSkipPanels = gPrefs.getCharPref(skipPanelsPrefStr);

    if (identity.smtpServerKey) {
      var smtpServer = smtpService.getServerByKey(identity.smtpServerKey);
      accountData.smtp = smtpServer;

      var smtpRequiresUsername = false;
      var smtpRequiresPrefStr = "mail.identity." + identity.key + ".smtpRequiresUsername";
      smtpRequiresUsername = gPrefs.getBoolPref(smtpRequiresPrefStr);
      accountData.smtpRequiresUsername = smtpRequiresUsername;
    }
  }
  catch(ex) {
    // reached here as special identity pre-configuration prefs 
    // (wizardSkipPanels, smtpRequiresUsername) are not defined.
  }

  return accountData;
}

function AccountToAccountData(account, defaultAccountData)
{
    dump("AccountToAccountData(" + account + ", " +
         defaultAccountData + ")\n");
    var accountData = defaultAccountData;
    if (!accountData)
        accountData = new Object;
    
    accountData.incomingServer = account.incomingServer;
    accountData.identity = account.identities.QueryElementAt(0, nsIMsgIdentity);
    accountData.smtp = smtpService.defaultServer;

    return accountData;
}

// sets the page data, automatically creating the arrays as necessary
function setPageData(pageData, tag, slot, value) {
    if (!pageData[tag]) pageData[tag] = [];

    if (value == undefined) {
        // clear out this slot
        if (pageData[tag][slot]) delete pageData[tag][slot];
    } else {
        // pre-fill this slot
        if (!pageData[tag][slot]) pageData[tag][slot] = [];
        pageData[tag][slot].id = slot;
        pageData[tag][slot].value = value;
    }
}

// value of checkbox on the first page
function serverIsNntp(pageData) {
  if (pageData.accounttype.newsaccount)
    return pageData.accounttype.newsaccount.value;
  return false;
}

function getUsernameFromEmail(email)
{
	var emailData = email.split("@");
    return emailData[0];
}

function getCurrentUserName(pageData)
{
	var userName = "";

	if (pageData.login) {
    	if (pageData.login.username) {
        	userName = pageData.login.username.value;
		}
	}
	if (userName == "") {
		var email = pageData.identity.email.value;
		userName = getUsernameFromEmail(email); 
	}
	return userName;
}

function getCurrentServerType(pageData) {
    var servertype = "pop3";    // hopefully don't resort to default!
    if (serverIsNntp(pageData))
        servertype = "nntp";
    else if (pageData.server && pageData.server.servertype)
        servertype = pageData.server.servertype.value;
    return servertype;
}

function getCurrentServerIsDeferred(pageData) {
    var serverDeferred = false; 
    if (getCurrentServerType(pageData) == "pop3" && pageData.server && pageData.server.deferStorage)
        serverDeferred = pageData.server.deferStorage.value;

    return serverDeferred;
}

function getCurrentHostname(pageData) {
    if (serverIsNntp(pageData))
        return pageData.newsserver.hostname.value;
    else
        return pageData.server.hostname.value;
}

function GetPageData()
{
    if (!gPageData)
      gPageData = new Object;

    return gPageData;
}

function PrefillAccountForIsp(ispName)
{
    dump("AccountWizard.prefillAccountForIsp(" + ispName + ")\n");

    var ispData = getIspDefaultsForUri(ispName);
    
    var pageData = GetPageData();

    if (!ispData) {
      SetCurrentAccountData(null);
      return;
    }

    // prefill the rest of the wizard
    dump("PrefillAccountForISP: filling with " + ispData + "\n");
    SetCurrentAccountData(ispData);
    AccountDataToPageData(ispData, pageData);
}

// does any cleanup work for the the account data
// - sets the username from the email address if it's not already set
// - anything else?
function FixupAccountDataForIsp(accountData)
{
    // no fixup for news
    // setting the username does bad things
    // see bugs #42105 and #154213
    if (accountData.incomingServer.type == "nntp")
      return;

    var email = accountData.identity.email;
    var username;

    if (email) {
      username = getUsernameFromEmail(email);
    }
    
    // fix up the username
    if (!accountData.incomingServer.username) {
        accountData.incomingServer.username = username;
    }

    if (!accountData.smtp.username &&
        accountData.smtpRequiresUsername) {
      // fix for bug #107953
      // if incoming hostname is same as smtp hostname
      // use the server username (instead of the email username)
      if (accountData.smtp.hostname == accountData.incomingServer.hostName)
        accountData.smtp.username = accountData.incomingServer.username;
      else
        accountData.smtp.username = username;
    }
}

function SetCurrentAccountData(accountData)
{
    //    dump("Setting current account data (" + gCurrentAccountData + ") to " + accountData + "\n");
    gCurrentAccountData = accountData;
}

function getInterfaceForType(type) {
    try {
        var protocolInfoContractIDPrefix = "@mozilla.org/messenger/protocol/info;1?type=";
        
        var thisContractID = protocolInfoContractIDPrefix + type;
        
        var protoInfo = Components.classes[thisContractID].getService(Components.interfaces.nsIMsgProtocolInfo);
        
        return protoInfo.serverIID;
    } catch (ex) {
        dump("could not get IID for " + type + ": " + ex + "\n");
        return undefined;
    }
}

// flush the XUL cache - just for debugging purposes - not called
function onFlush() {
        gPrefs.setBoolPref("nglayout.debug.disable_xul_cache", true);
        gPrefs.setBoolPref("nglayout.debug.disable_xul_cache", false);

}

/** If there are no default accounts..
  * this is will be the new default, so enable
  * check for mail at startup
  */
function EnableCheckMailAtStartUpIfNeeded(newAccount)
{
    // Check if default account exists and if that account is alllowed to be
    // a default account. If no such account, make this one as the default account 
    // and turn on the new mail check at startup for the current account   
    if (!(gDefaultAccount && gDefaultAccount.incomingServer.canBeDefaultServer)) { 
        am.defaultAccount = newAccount;
        newAccount.incomingServer.loginAtStartUp = true;
        newAccount.incomingServer.downloadOnBiff = true;
    }
}

function SetSmtpRequiresUsernameAttribute(accountData) 
{
    // If this is the default server, time to set the smtp user name
    // Set the generic attribute for requiring user name for smtp to true.
    // ISPs can override the pref via rdf files.
    if (!(gDefaultAccount && gDefaultAccount.incomingServer.canBeDefaultServer)) { 
        accountData.smtpRequiresUsername = true;
    }
}

function setNextPage(currentPageId, nextPageId) {
    var currentPage = document.getElementById(currentPageId);
    currentPage.next = nextPageId;
}

