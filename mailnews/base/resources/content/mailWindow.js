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

 //This file stores variables common to mail windows
var messengerContractID        = "@mozilla.org/messenger;1";
var statusFeedbackContractID   = "@mozilla.org/messenger/statusfeedback;1";
var messageViewContractID      = "@mozilla.org/messenger/messageview;1";
var mailSessionContractID      = "@mozilla.org/messenger/services/session;1";
var secureUIContractID         = "@mozilla.org/secure_browser_ui;1";


var prefContractID             = "@mozilla.org/preferences;1";
var msgWindowContractID		   = "@mozilla.org/messenger/msgwindow;1";

var messenger;
var pref;
var statusFeedback;
var messageView;
var msgWindow;

var msgComposeService;
var accountManager;
var RDF;
var msgComposeType;
var msgComposeFormat;

var mailSession;

var Bundle;
var BrandBundle;

var datasourceContractIDPrefix = "@mozilla.org/rdf/datasource;1?name=";
var accountManagerDSContractID = datasourceContractIDPrefix + "msgaccountmanager";
var folderDSContractID         = datasourceContractIDPrefix + "mailnewsfolders";
var messageDSContractID        = datasourceContractIDPrefix + "mailnewsmessages";

var accountManagerDataSource;
var folderDataSource;
var messageDataSource;

//End progress and Status variables

function OnMailWindowUnload()
{
	dump("we get here\n");
	var mailSession = Components.classes[mailSessionContractID].getService();
	if(mailSession)
	{
		mailSession = mailSession.QueryInterface(Components.interfaces.nsIMsgMailSession);
		if(mailSession)
		{
			mailSession.RemoveFolderListener(folderListener);
		}
	}

	mailSession.RemoveMsgWindow(msgWindow);
	messenger.SetWindow(null, null);

	var msgDS = folderDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = null;

	msgDS = messageDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = null;

	msgDS = accountManagerDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = null;


  	msgWindow.closeWindow();

}

function CreateMailWindowGlobals()
{
	// get the messenger instance
	messenger = Components.classes[messengerContractID].createInstance();
	messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);

	pref = Components.classes[prefContractID].getService(Components.interfaces.nsIPref);

	//Create windows status feedback
  // set the JS implementation of status feedback before creating the c++ one..
  window.MsgStatusFeedback = new nsMsgStatusFeedback();
  // double register the status feedback object as the xul browser window implementation 
  window.XULBrowserWindow = window.MsgStatusFeedback;

	statusFeedback           = Components.classes[statusFeedbackContractID].createInstance();
	statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

  // try to create and register ourselves with a security icon...
  var securityIcon = document.getElementById("security-button");
  if (securityIcon)
  {
    // if the client isn't built with psm enabled then we won't have a secure UI to monitor the lock icon
    // so be sure to wrap this in a try / catch clause...
    try 
    {
      var secureUI = Components.classes[secureUIContractID].createInstance();
      // we may not have a secure UI if psm isn't installed!
      if (secureUI)
      {
        secureUI = secureUI.QueryInterface(Components.interfaces.nsSecureBrowserUI);
        secureUI.init(_content, securityIcon);
      }
    }
    catch (ex) {}
  }

	window.MsgWindowCommands = new nsMsgWindowCommands();
	//Create message view object
	messageView = Components.classes[messageViewContractID].createInstance();
	messageView = messageView.QueryInterface(Components.interfaces.nsIMessageView);

	//Create message window object
	msgWindow = Components.classes[msgWindowContractID].createInstance();
	msgWindow = msgWindow.QueryInterface(Components.interfaces.nsIMsgWindow);

	msgComposeService = Components.classes['@mozilla.org/messengercompose;1'].getService();
	msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

	mailSession = Components.classes["@mozilla.org/messenger/services/session;1"].getService(Components.interfaces.nsIMsgMailSession); 

	accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);

	RDF = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

	msgComposeType = Components.interfaces.nsIMsgCompType;
	msgComposeFormat = Components.interfaces.nsIMsgCompFormat;

	Bundle = srGetStrBundle("chrome://messenger/locale/messenger.properties");
  BrandBundle = srGetStrBundle("chrome://global/locale/brand.properties");

	//Create datasources
	accountManagerDataSource = Components.classes[accountManagerDSContractID].createInstance();
	folderDataSource         = Components.classes[folderDSContractID].createInstance();
	messageDataSource        = Components.classes[messageDSContractID].createInstance();

}

function InitMsgWindow()
{
	msgWindow.statusFeedback = statusFeedback;
	msgWindow.messageView = messageView;
	msgWindow.msgHeaderSink = messageHeaderSink;
	msgWindow.SetDOMWindow(window);
	mailSession.AddMsgWindow(msgWindow);

}

function AddDataSources()
{

	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	//to move menu item
	SetupMoveCopyMenus('moveMenu', accountManagerDataSource, folderDataSource);

	//to copy menu item
	SetupMoveCopyMenus('copyMenu', accountManagerDataSource, folderDataSource);


	//To FileButton menu
	SetupMoveCopyMenus('FileButtonMenu', accountManagerDataSource, folderDataSource);

	//To move and copy menus in message pane context
	SetupMoveCopyMenus("messagePaneContext-copyMenu", accountManagerDataSource, folderDataSource);
	SetupMoveCopyMenus("messagePaneContext-moveMenu", accountManagerDataSource, folderDataSource);

	//Add statusFeedback

	var msgDS = folderDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = msgWindow;

	msgDS = messageDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = msgWindow;

	msgDS = accountManagerDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = msgWindow;

}	

function SetupMoveCopyMenus(menuid, accountManagerDataSource, folderDataSource)
{
	var menu = document.getElementById(menuid);
	if(menu)
	{
		menu.database.AddDataSource(accountManagerDataSource);
		menu.database.AddDataSource(folderDataSource);
		menu.setAttribute('ref', 'msgaccounts:/');
	}
}
function dumpProgress() {
    var broadcaster = document.getElementById("Messenger:LoadingProgress");
    dump( "broadcaster mode=" + broadcaster.getAttribute("mode") + "\n" );
    dump( "broadcaster value=" + broadcaster.getAttribute("value") + "\n" );
    dump( "meter mode=" + meter.getAttribute("mode") + "\n" );
    dump( "meter value=" + meter.getAttribute("value") + "\n" );
}

// We're going to implement our status feedback for the mail window in JS now.
// the following contains the implementation of our status feedback object

function nsMsgStatusFeedback()
{
}

nsMsgStatusFeedback.prototype = 
{
  // global variables for status / feedback information....
  startTime  : 0,
  statusTextFld : null,
  statusBar     : null,
  throbber      : null,
  stopMenu      : null,
  stopButton    : null,

  ensureStatusFields : function()
    {
      if (!this.statusTextFld ) this.statusTextFld = document.getElementById("statusText");
      if (!this.statusBar) this.statusBar = document.getElementById("statusbar-icon");
      if(!this.throbber)   this.throbber = document.getElementById("navigator-throbber");
	    if(!this.stopButton) this.stopButton = document.getElementById("button-stop");
	    if(!this.stopMenu)   this.stopMenu = document.getElementById("stopMenuitem");
    },

  // nsIXULBrowserWindow implementation
  setJSStatus : function(status)
    {
    },
  setJSDefaultStatus : function(status)
    {
    },
  setDefaultStatus : function(status)
    {
    },
  setOverLink : function(link)
    {
      this.showStatusString(link);
    },
  onProgress : function (channel, current, max)
    {
    },
  onStateChange : function (progress, request, state, status)
    {
    },
  onStatus : function(channel, url, message)
    {
    },
  onLocationChange : function(location)
    {
    },

	QueryInterface : function(iid)
		{
	    if(iid.equals(Components.interfaces.nsIMsgStatusFeedback))
		    return this;
      if(iid.equals(Components.interfaces.nsIXULBrowserWindow))
        return this;
	    throw Components.results.NS_NOINTERFACE;
      return null;
		},

  // nsIMsgStatusFeedback implementation.
	showStatusString : function(statusText)
		{
       this.ensureStatusFields();

       if ( statusText == "" )
          statusText = defaultStatus;
       this.statusTextFld.value = statusText;
		},
	startMeteors : function()
		{
      this.ensureStatusFields();

      // Turn progress meter on.
      this.statusBar.setAttribute("mode","undetermined");
      
      // turn throbber on 
      this.throbber.setAttribute("busy", true);

	    //turn on stop button and menu
	    this.stopButton.setAttribute("disabled", false);
	    this.stopMenu.setAttribute("disabled", false);
      
      // Remember when loading commenced.
    	this.startTime = (new Date()).getTime();
		},
	stopMeteors : function()
		{
      this.ensureStatusFields();

			// Record page loading time.
			var elapsed = ( (new Date()).getTime() - this.startTime ) / 1000;
      var msg = Bundle.GetStringFromName("documentDonePrefix") +
                elapsed + Bundle.GetStringFromName("documentDonePostfix");

      this.showStatusString(msg);
      defaultStatus = msg;

      this.throbber.setAttribute("busy", false);

      // Turn progress meter off.
      this.statusBar.setAttribute("mode","normal");
      this.statusBar.value = 0;  // be sure to clear the progress bar
      this.statusBar.progresstext = "";
	    this.stopButton.setAttribute("disabled", true);
	    this.stopMenu.setAttribute("disabled", true);

		},
	showProgress : function(percentage)
		{
      this.ensureStatusFields();
      if (percentage >= 0)
      {
        this.statusBar.setAttribute("mode", "normal");
        this.statusBar.value = percentage; 
        this.statusBar.progresstext = Math.round(percentage) + "%";
      }
		},
	closeWindow : function(percent)
		{
		}
}


function nsMsgWindowCommands()
{
}

nsMsgWindowCommands.prototype = 
{
	QueryInterface : function(iid)
	{
		if(iid.equals(Components.interfaces.nsIMsgWindowCommands))
			return this;
		throw Components.results.NS_NOINTERFACE;
        return null;
	},
	SelectFolder: function(folderUri)
	{

		SelectFolder(folderUri);

	},
	SelectMessage: function(messageUri)
	{
		SelectMessage(messageUri);
	}
}

function StopUrls()
{
	msgWindow.StopUrls();
}

function loadStartPage() {
    try {
		var startpageenabled= pref.GetBoolPref("mailnews.start_page.enabled");
        
		if (startpageenabled) {
			var startpage = pref.getLocalizedUnicharPref("mailnews.start_page.url");
            if (startpage != "") {
                window.frames["messagepane"].location = startpage;
                dump("start message pane with: " + startpage + "\n");
				ClearMessageSelection();
            }
        }
	}
    catch (ex) {
        dump("Error loading start page.\n");
        return;
    }
}

