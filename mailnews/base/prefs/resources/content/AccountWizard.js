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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

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

var gWizardMap = {
    accounttype: { next: "identity" },
    identity: { previous: "accounttype"}, // don't define next: server/newsserver
    server:   { next: "login", previous: "identity"},
    newsserver: { next: "accname", previous: "identity"},
    login:    { next: "accname", previous: "server"}, 
    accname:  { next: "done", }, // don't define previous: login/newsserver
    done:     { previous: "accname", finish: true }
}

var pagePrefix="chrome://messenger/content/aw-";
var pagePostfix=".xul";

var currentPageTag;

var contentWindow;

var smtpService;
var am;

var nsIMsgIdentity = Components.interfaces.nsIMsgIdentity;
var nsIMsgIncomingServer = Components.interfaces.nsIMsgIncomingServer;

// the current nsIMsgAccount
var gCurrentAccount;

// the current associative array that
// will eventually be dumped into the account
var gCurrentAccountData;

// event handlers
function onLoad() {

    // wizard stuff
    // instantiate the Wizard Manager
    wizardManager = new WizardManager( "wizardContents", null, null,
                                       gWizardMap );
    wizardManager.URL_PagePrefix = "chrome://messenger/content/aw-";
    wizardManager.URL_PagePostfix = ".xul"; 
    wizardManager.SetHandlers(null, null, onFinish, null, null, null);

    // load up the SMTP service for later
    if (!smtpService) {
        smtpService =
            Components.classes["component://netscape/messengercompose/smtp"].getService(Components.interfaces.nsISmtpService);
    }

    checkForInvalidAccounts();
    var pageData = GetPageData();
    updateMap(pageData, gWizardMap);

    // skip the first page if we have an account
    if (gCurrentAccount) {
        // skip past first pane
        gWizardMap.identity.previous = null;
        wizardManager.LoadPage("identity", false);
    }
    else
        wizardManager.LoadPage("accounttype", false);
}

    

function onFinish() {
    if( !wizardManager.wizardMap[wizardManager.currentPageTag].finish )
        return;
    var pageData = GetPageData();

    dump(parent.wizardManager.WSM);

    var accountData= gCurrentAccountData;
    
    if (!accountData)
        accountData = new Object;
    
    PageDataToAccountData(pageData, accountData);

    FixupAccountDataForIsp(accountData);
    
    // we might be simply finishing another account
    if (!gCurrentAccount)
        gCurrentAccount = createAccount(accountData);

    // transfer all attributes from the accountdata
    finishAccount(gCurrentAccount, accountData);
    
    verifyLocalFoldersAccount(gCurrentAccount);

    // hack hack - save the prefs file NOW in case we crash
    try {
        var prefs = Components.classes["component://netscape/preferences"].getService(Components.interfaces.nsIPref);
        prefs.SavePrefFile();
    } catch (ex) {
        dump("Error saving prefs!\n");
		dump("ex = " + ex + "\n");
    }
    window.close();
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
        setPageData(pageData, "accounttype", "mailaccount", undefined);
        setPageData(pageData, "accounttype", "newsaccount", undefined);
        setPageData(pageData, "server", "servertype", undefined);
        setPageData(pageData, "server", "hostname", undefined);
        
    } else {
        
        if (server.type == "nntp") {
            setPageData(pageData, "accounttype", "newsaccount", true);
            setPageData(pageData, "newsserver", "hostname", server.hostName);
        }
        
        else {
            setPageData(pageData, "accounttype", "mailaccount", true);
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
        setPageData(pageData, "server", "smtphostname",
                    smtp.hostname);
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
    
    var identity = accountData.identity;
    var server = accountData.incomingServer;
    var smtp = accountData.smtp;

    dump("Setting identity for " + pageData.identity.email.value + "\n");
    identity.email = pageData.identity.email.value;
    identity.fullName = pageData.identity.fullName.value;

    server.type = getCurrentServerType(pageData);
    server.hostName = getCurrentHostname(pageData);

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
        }

        dump("pageData.server = " + pageData.server + "\n");
        if (pageData.server) {
            dump("pageData.server.smtphostname.value = " + pageData.server.smtphostname + "\n");
            if (pageData.server.smtphostname &&
                pageData.server.smtphostname.value)
                smtp.hostname = pageData.server.smtphostname.value;
        }
    }
    
    server.prettyName = pageData.accname.prettyName.value;

}

// given an accountData structure, create an account
// (but don't fill in any fields, that's for finishAccount()
function createAccount(accountData)
{

    var server = accountData.incomingServer;
    dump("am.createIncomingServer(" + server.username + "," +
                                      server.hostName + "," +
                                      server.type + ")\n");
    var server = am.createIncomingServer(server.username,
                                         server.hostName,
                                         server.type);
    
    dump("am.createIdentity()\n");
    var identity = am.createIdentity();
    
    /* new nntp identities should use plain text by default
     * we want that GNKSA (The Good Net-Keeping Seal of Approval) */
    if (server.type == "nntp") {
			identity.composeHtml = false;
    }

    dump("am.createAccount()\n");
    var account = am.createAccount();
    account.addIdentity(identity);
    account.incomingServer = server;
    return account;
}

// given an accountData structure, copy the data into the
// given account, incoming server, and so forth
function finishAccount(account, accountData) {

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
    }

    // copy identity info
    var destIdentity =
        account.identities.QueryElementAt(0, nsIMsgIdentity);
    
    if (accountData.identity && destIdentity) {

        // fixup the email address if we have a default domain
        var emailArray = accountData.identity.email.split('@');
        if (emailArray.length < 2 && accountData.domain) {
            accountData.identity.email += '@' + accountData.domain;
        }

        copyObjectToInterface(destIdentity,
                              accountData.identity);
        destIdentity.valid=true;
    }

    // don't try to create an smtp server if we already have one.
    if (!destIdentity.smtpServerKey)
    {
      var smtpServer = smtpService.defaultServer;
        
      if (accountData.smtpCreateNewServer)
          smtpServer = smtpService.createSmtpServer();

      dump("Copying smtpServer (" + smtpServer + ") to accountData\n");
      copyObjectToInterface(smtpServer, accountData.smtp);

      // some identities have 'preferred' 
      if (accountData.smtpUsePreferredServer && destIdentity)
          destIdentity.smtpServerKey = smtpServer.key;
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
            dump("(This is ok if this is a ServerType-* attribute)\n");
        }
    }
}

// check if there already is a "Local Folders"
// if not, create it.
function verifyLocalFoldersAccount(account) {
    
    dump("Looking for Local Folders.....\n");
	var localMailServer = null;
	try {
		localMailServer = am.localFoldersServer;
	}
	catch (ex) {
        // dump("exception in findserver: " + ex + "\n");
		localMailServer = null;
	}

    try {
    var server = account.incomingServer;
    var identity = account.identities.QueryElementAt(0, Components.interfaces.nsIMsgIdentity);

	// use server type to get the protocol info
	var protocolinfo = Components.classes["component://netscape/messenger/protocol/info;type=" + server.type].getService(Components.interfaces.nsIMsgProtocolInfo);
	// for this protocol, do we default the folder prefs to this server, or to the "Local Folders" server
	defaultCopiesAndFoldersPrefsToServer = protocolinfo.defaultCopiesAndFoldersPrefsToServer;

	if (!localMailServer) {
        	// dump("Creating local mail account\n");
		// creates a copy of the identity you pass in
        	messengerMigrator = Components.classes["component://netscape/messenger/migrator"].getService(Components.interfaces.nsIMessengerMigrator);
		messengerMigrator.createLocalMailAccount(false /* false, since we are not migrating */);
		try {
			localMailServer = am.localFoldersServer;
		}
		catch (ex) {
			dump("error!  we should have found the local mail server after we created it.\n");
			localMailServer = null;
		}	
    	}

	var copiesAndFoldersServer = null;
	if (defaultCopiesAndFoldersPrefsToServer) {
		copiesAndFoldersServer = server;
	}
	else {
		if (!localMailServer) {
			dump("error!  we should have a local mail server at this point\n");
			return;
		}
		copiesAndFoldersServer = localMailServer;
	}
	
	setDefaultCopiesAndFoldersPrefs(identity, copiesAndFoldersServer);

    } catch (ex) {
        // return false (meaning we did not create the account)
        // on any error
        dump("Error creating local mail: " + ex + "\n");
        return false;
    }
    return true;
}

function setDefaultCopiesAndFoldersPrefs(identity, server)
{
	dump("finding folders on server = " + server.hostName + "\n");

	var rootFolder = server.RootFolder;

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

function checkForInvalidAccounts()
{
    am = Components.classes["component://netscape/messenger/account-manager"].getService(Components.interfaces.nsIMsgAccountManager);

    var invalidAccounts = getInvalidAccounts(am.accounts);
    var firstInvalidAccount; 
  
    if (invalidAccounts.length > 0)
        firstInvalidAccount = invalidAccounts[0];
    else
        return null;

    if (firstInvalidAccount) {
        var pageData = GetPageData();
        dump("We have an invalid account, " + firstInvalidAccount + ", let's use that!\n");
        gCurrentAccount = firstInvalidAccount;

        // there's a possibility that the invalid account has ISP defaults
        // as well.. so first pre-fill accountData with ISP info, then
        // overwrite it with the account data


        var identity =
            firstInvalidAccount.identities.QueryElementAt(0, nsIMsgIdentity);

        dump("Invalid account: trying to get ISP data for " + identity.email + "\n");
        var accountData = getIspDefaultsForEmail(identity.email);
        dump("Invalid account: Got " + accountData + "\n");
        
        // account -> accountData -> pageData
        AccountToAccountData(firstInvalidAccount, accountData);
        AccountDataToPageData(accountData, pageData);

        gCurrentAccountData = accountData;
        
        dump(parent.wizardManager.WSM);
    }
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

function getCurrentServerType(pageData) {
    var servertype = "pop3";    // hopefully don't resort to default!
    if (serverIsNntp(pageData))
        servertype = "nntp";
    else if (pageData.server && pageData.server.servertype)
        servertype = pageData.server.servertype.value;
    return servertype;
}

function getCurrentHostname(pageData) {
    if (serverIsNntp(pageData))
        return pageData.newsserver.hostname.value;
    else
        return pageData.server.hostname.value;
}

function UpdateWizardMap() {
    updateMap(GetPageData(), gWizardMap);
}

// updates the map based on various odd states
// conditions handled right now:
// - 
function updateMap(pageData, wizardMap) {
    dump("Updating wizard map..\n");
    if (pageData.accounttype) {
        var ismailaccount = pageData.accounttype.mailaccount
        dump("Accounttype is mail: " + (ismailaccount && ismailaccount.value) + "\n");
        // set up default account stuff
        wizardMap.identity.next = "server";
        wizardMap.done.previous = "accname";
        
        if (pageData.accounttype.mailaccount &&
            pageData.accounttype.mailaccount.value) {

            wizardMap.accname.previous = "login";
            
            if (gCurrentAccountData && gCurrentAccountData.wizardSkipPanels) {
                wizardMap.identity.next = "done";
                wizardMap.done.previous = "identity";
            }
        }

        else if (pageData.accounttype.newsaccount &&
                 pageData.accounttype.newsaccount.value) {
            wizardMap.identity.next = "newsserver";
            wizardMap.accname.previous = "newsserver";
        }
        else {
            dump("Handle other types (" + pageData.accounttype + ") here?\n");
        }
    }

}

function GetPageData()
{
    return parent.wizardManager.WSM.PageData;
}
    

function PrefillAccountForIsp(ispName)
{
    dump("AccountWizard.prefillAccountForIsp(" + ispName + ")\n");

    var ispData = getIspDefaultsForUri(ispName);
    
    var pageData = GetPageData();


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
    var email = accountData.identity.email;
    var username;

    if (email) {
        var emailData = email.split("@");
        username = emailData[0];
    }
    
    // fix up the username
    if (!accountData.incomingServer.username) {
        accountData.incomingServer.username = username;
    }

    if (!accountData.smtp.username &&
        accountData.smtpRequiresUsername) {
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
        var protocolInfoProgIDPrefix = "component://netscape/messenger/protocol/info;type=";
        
        var thisProgID = protocolInfoProgIDPrefix + type;
        
        var protoInfo = Components.classes[thisProgID].getService(Components.interfaces.nsIMsgProtocolInfo);
        
        return protoInfo.serverIID;
    } catch (ex) {
        dump("could not get IID for " + type + ": " + ex + "\n");
        return undefined;
    }
}
