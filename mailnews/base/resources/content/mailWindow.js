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

//Progress and Status variables
var gStatusText;
var gStatusBar;
var gThrobber;
var gStopMenu;
var gStopButton;
var bindCount = 0;
var startTime = 0;
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
	statusFeedback           = Components.classes[statusFeedbackContractID].createInstance();
	statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

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

    dump( "bindCount=" + bindCount + "\n" );
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
	QueryInterface : function(iid)
		{
		if(iid.equals(Components.interfaces.nsIMsgStatusFeedback))
			return this;
		throw Components.results.NS_NOINTERFACE;
        return null;
		},
	ShowStatusString : function(statusText)
		{
       if (!gStatusText ) gStatusText = document.getElementById("statusText");
       if ( statusText == "" )
          statusText = defaultStatus;
       gStatusText.value = statusText;
		},
	StartMeteors : function()
		{
      if (!gStatusBar) gStatusBar = document.getElementById("statusbar-icon");
      if(!gThrobber) gThrobber = document.getElementById("navigator-throbber");
	  if(!gStopButton) gStopButton = document.getElementById("button-stop");
	  if(!gStopMenu) gStopMenu = document.getElementById("stopMenuitem");

      // Turn progress meter on.
      gStatusBar.setAttribute("mode","undetermined");
      
      // turn throbber on 
      gThrobber.setAttribute("busy", true);

	  //turn on stop button and menu
	  gStopButton.setAttribute("disabled", false);
	  gStopMenu.setAttribute("disabled", false);
      
      // Remember when loading commenced.
    	startTime = (new Date()).getTime();
		},
	StopMeteors : function()
		{
            dump("stopping meteors 1\n");
            if (!gStatusBar)
                gStatusBar = document.getElementById("statusbar-icon");
            if(!gThrobber)
                gThrobber = document.getElementById("navigator-throbber");
			if(!gStopButton) gStopButton = document.getElementById("button-stop");
			if(!gStopMenu) gStopMenu = document.getElementById("stopMenuitem");

			// Record page loading time.
			var elapsed = ( (new Date()).getTime() - startTime ) / 1000;
            var msg = Bundle.GetStringFromName("documentDonePrefix") +
                elapsed + Bundle.GetStringFromName("documentDonePostfix");
			dump( msg + "\n" );
      window.MsgStatusFeedback.ShowStatusString(msg);
      defaultStatus = msg;

      gThrobber.setAttribute("busy", false);
      dump("stopping meteors\n");
      // Turn progress meter off.
      gStatusBar.setAttribute("mode","normal");
      gStatusBar.value = 0;  // be sure to clear the progress bar
      gStatusBar.progresstext = "";
	  gStopButton.setAttribute("disabled", true);
	  gStopMenu.setAttribute("disabled", true);

		},
	ShowProgress : function(percentage)
		{
      if (!gStatusBar) gStatusBar = document.getElementById("statusbar-icon");
      if (percentage >= 0)
      {
        gStatusBar.setAttribute("mode", "normal");
        gStatusBar.value = percentage; 
        gStatusBar.progresstext = Math.round(percentage) + "%";
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
			var startpage = pref.CopyCharPref("mailnews.start_page.url");
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

