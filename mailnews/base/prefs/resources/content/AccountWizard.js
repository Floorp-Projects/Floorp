/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

var wizardMap = {
    accounttype: { next: "identity", finish: false},
    identity: { next: "server", previous: "accounttype"},
    server:   { next: "login", previous: "identity"},
    login:    { next: "accname", previous: "server"},
    accname:  { next: "done", previous: "login" },
    done:     { previous: "accname", finish: true }
}

var pagePrefix="chrome://messenger/content/aw-";
var pagePostfix=".xul";

var currentPageTag;

var contentWindow;

var smtpService;

// event handlers
function onLoad() {
    // wizard stuff
    // instantiate the Wizard Manager
    wizardManager = new WizardManager( "wizardContents", null, null,
                                       wizardMap );
    wizardManager.URL_PagePrefix = "chrome://messenger/content/aw-";
    wizardManager.URL_PagePostfix = ".xul"; 
    wizardManager.SetHandlers(null, null, onFinish, null, null, null);

    // load up the SMTP service for later
    if (!smtpService) {
        smtpService =
            Components.classes["component://netscape/messengercompose/smtp"].getService(Components.interfaces.nsISmtpService);;
    }
    
    wizardManager.LoadPage("accounttype", false);
}

function onFinish() {
    var pageData = parent.wizardManager.WSM.PageData;

    dump(parent.wizardManager.WSM);
    var account = createAccount(pageData);

    if (account)
        verifyLocalFoldersAccount(account);

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

function createAccount(pageData) {

  try {
    var am = Components.classes["component://netscape/messenger/account-manager"].getService(Components.interfaces.nsIMsgAccountManager);

    // the following fields are used to create the account
    var fullname = pageData.identity.fullName.value; 
    var email    = pageData.identity.email.value;
    var hostname = pageData.server.hostname.value;
    var servertype = pageData.server.servertype.value;
    var smtphostname = pageData.server.smtphostname.value;
    var username = pageData.login.username.value;
    var rememberPassword = pageData.login.rememberPassword.value;
    var password = pageData.login.password.value;
    var prettyName = pageData.accname.prettyName.value;
        
    // workaround for lame-ass combo box bug
    if (!servertype  || servertype == "")
        servertype = "pop3";
        
    dump("am.createIncomingServer(" + username + "," +
                                      hostname + "," +
                                      servertype + "\n");
    var server = am.createIncomingServer(username, hostname, servertype);
    server.rememberPassword = rememberPassword;
    server.password = password;
    server.prettyName = prettyName;
    
    dump("am.createIdentity()\n");
    var identity = am.createIdentity();
    identity.email = email;
    identity.fullName = fullname;

    /* new nntp identities should use plain text by default
     * we wan't that GNKSA (The Good Net-Keeping Seal of Approval) */
    if (servertype == "nntp") {
			identity.composeHtml = false;
    }

    dump("am.createAccount()\n");
    var account = am.createAccount();
    account.incomingServer = server;
    account.addIdentity(identity);

    dump("Setting SMTP server..\n");
    smtpService.defaultServer.hostname = smtphostname;
  }
  
  catch (ex) {
      dump("Error creating account: " + ex + "\n");
      return null;
  }
  
  return account;
}

// check if there already is a "none" account. (aka "Local Folders")
// if not, create it.
function verifyLocalFoldersAccount(account) {
    
    dump("Account is " + account + "\n");
    dump("Server is " + account.server + "\n");
    dump("Identity is " + account.identity + "\n");
    
    var am = Components.classes["component://netscape/messenger/account-manager"].getService(Components.interfaces.nsIMsgAccountManager);
    dump("Looking for local mail..\n");
	var localMailServer = null;
	try {
		// look for anything that is of type "none".
		// "none" is the type for "Local Mail"
		localMailServer = am.FindServer("","","none");
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
    }

	var copiesAndFoldersServer = null;
	if (defaultCopiesAndFoldersPrefsToServer) {
		copiesAndFoldersServer = server;
	}
	else {
		if (!localMailServer) {
			dump("error!  we should have a local mail server at this point");
			return;
		}
		copiesAndFoldersServer = localMailServer;
	}
	
	setDefaultCopiesAndFoldersPrefs(identity, copiesAndFoldersServer);

    return true;

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

	// we need to do this or it is possible that the server's draft, stationery fcc folder
	// will not be in rdf
	//
	// this can happen in a couple cases
	// 1) the first account we create, creates the local mail.  since local mail
	// was just created, it obviously hasn't been opened, or in rdf..
	// 2) the account we created is of a type where defaultCopiesAndFoldersPrefsToServer is true
	// this since we are creating the server, it obviously hasn't been opened, or in rdf.
	//
	// this makes the assumption that the server's draft, stationery fcc folder
	// are at the top level (ie subfolders of the root folder.)  this works because
	// we happen to be doing things that way, and if the user changes that, it will
	// work because to change the folder, it must be in rdf, coming from the folder cache.
	// in the worst case.
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

	return;
}
