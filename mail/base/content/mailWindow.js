# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributors(s):
#   Jan Varga <varga@nixcorp.com>
#   Håkan Waara (hwaara@chello.se)

//This file stores variables common to mail windows
var messengerContractID        = "@mozilla.org/messenger;1";
var statusFeedbackContractID   = "@mozilla.org/messenger/statusfeedback;1";
var mailSessionContractID      = "@mozilla.org/messenger/services/session;1";
var secureUIContractID         = "@mozilla.org/secure_browser_ui;1";


var prefContractID             = "@mozilla.org/preferences-service;1";
var msgWindowContractID      = "@mozilla.org/messenger/msgwindow;1";

var messenger;
var pref;
var prefServices;
var statusFeedback;
var messagePaneController;
var msgWindow;

var msgComposeService;
var accountManager;
var RDF;
var msgComposeType;
var msgComposeFormat;

var mailSession;

var gMessengerBundle;
var gBrandBundle;

var datasourceContractIDPrefix = "@mozilla.org/rdf/datasource;1?name=";
var accountManagerDSContractID = datasourceContractIDPrefix + "msgaccountmanager";
var folderDSContractID         = datasourceContractIDPrefix + "mailnewsfolders";

var accountManagerDataSource;
var folderDataSource;

var accountCentralBox = null;
var gSearchBox = null;
var gAccountCentralLoaded = false;
var gFakeAccountPageLoaded = false;
//End progress and Status variables

// for checking if the folder loaded is Draft or Unsent which msg is editable
var gIsEditableMsgFolder = false;
var gOfflineManager;

// cache the last keywords
var gLastKeywords = "";

function OnMailWindowUnload()
{
  RemoveMailOfflineObserver();
  ClearPendingReadTimer();

  var searchSession = GetSearchSession();
  if (searchSession)
  {
    removeGlobalListeners();
    if (gPreQuickSearchView)     //close the cached pre quick search view
      gPreQuickSearchView.close();
  }
  
  var dbview = GetDBView();
  if (dbview) {
    dbview.close(); 
  }

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

  msgDS = accountManagerDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
  msgDS.window = null;


  msgWindow.closeWindow();
}

function CreateMessenger()
{
  messenger = Components.classes[messengerContractID].createInstance();
  messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);
}

function CreateMailWindowGlobals()
{
  // get the messenger instance
  CreateMessenger();

  prefServices = Components.classes[prefContractID].getService(Components.interfaces.nsIPrefService);
  pref = prefServices.getBranch(null);

  //Create windows status feedback
  // set the JS implementation of status feedback before creating the c++ one..
  window.MsgStatusFeedback = new nsMsgStatusFeedback();
  // double register the status feedback object as the xul browser window implementation
  window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
        .getInterface(Components.interfaces.nsIWebNavigation)
        .QueryInterface(Components.interfaces.nsIDocShellTreeItem).treeOwner
        .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
        .getInterface(Components.interfaces.nsIXULWindow)
        .XULBrowserWindow = window.MsgStatusFeedback;

  statusFeedback           = Components.classes[statusFeedbackContractID].createInstance();
  statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

  /*
    not in use unless we want the lock button back

  // try to create and register ourselves with a security icon...
  var securityIcon = document.getElementById("security-button");
  if (securityIcon) {
    // if the client isn't built with psm enabled then we won't have a secure UI to monitor the lock icon
    // so be sure to wrap this in a try / catch clause...
    try {
      var secureUI;
      // we may not have a secure UI if psm isn't installed!
      if (secureUIContractID in Components.classes) {
        secureUI = Components.classes[secureUIContractID].createInstance();
        if (secureUI) {
          secureUI = secureUI.QueryInterface(Components.interfaces.nsISecureBrowserUI);
          secureUI.init(_content, securityIcon);
        }
      }
    }
    catch (ex) {}
  }
  */

  window.MsgWindowCommands = new nsMsgWindowCommands();

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

  gMessengerBundle = document.getElementById("bundle_messenger");
  gBrandBundle = document.getElementById("bundle_brand");

  //Create datasources
  accountManagerDataSource = Components.classes[accountManagerDSContractID].createInstance();
  folderDataSource         = Components.classes[folderDSContractID].getService();

  accountCentralBox = document.getElementById("accountCentralBox");
  gSearchBox = document.getElementById("searchBox");
}

function InitMsgWindow()
{
  msgWindow.messagePaneController = new nsMessagePaneController();
  msgWindow.statusFeedback = statusFeedback;
  msgWindow.msgHeaderSink = messageHeaderSink;
  msgWindow.SetDOMWindow(window);
  mailSession.AddMsgWindow(msgWindow);
  document.getElementById("messagepane").docShell.allowAuth = false;
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
  SetupMoveCopyMenus('button-file', accountManagerDataSource, folderDataSource);

  //To move and copy menus in message pane context
  SetupMoveCopyMenus("messagePaneContext-copyMenu", accountManagerDataSource, folderDataSource);
  SetupMoveCopyMenus("messagePaneContext-moveMenu", accountManagerDataSource, folderDataSource);

  //Add statusFeedback

  var msgDS = folderDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
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

// We're going to implement our status feedback for the mail window in JS now.
// the following contains the implementation of our status feedback object

function nsMsgStatusFeedback()
{
}

nsMsgStatusFeedback.prototype =
{
  // global variables for status / feedback information....
  statusTextFld : null,
  statusBar     : null,
  throbber      : null,
  stopCmd       : null,
  startTimeoutID : null,
  stopTimeoutID  : null,
  progressMeterContainer : null,
  pendingStartRequests : 0,
  meteorsSpinning : false,
  progressMeterVisible : false,

  ensureStatusFields : function()
    {
      if (!this.statusTextFld ) this.statusTextFld = document.getElementById("statusText");
      if (!this.statusBar) this.statusBar = document.getElementById("statusbar-icon");
      if(!this.throbber)   this.throbber = document.getElementById("navigator-throbber");
      if(!this.stopCmd)   this.stopCmd = document.getElementById("cmd_stop");
      if (!this.progressMeterContainer) this.progressMeterContainer = document.getElementById("statusbar-progresspanel");
    },

  // nsIXULBrowserWindow implementation
  setJSStatus : function(status)
    {
    },
  setJSDefaultStatus : function(status)
    {
    },
  setOverLink : function(link)
    {
      this.showStatusString(link);
    },
  QueryInterface : function(iid)
    {
      if (iid.equals(Components.interfaces.nsIMsgStatusFeedback) ||
          iid.equals(Components.interfaces.nsIXULBrowserWindow) ||
          iid.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_NOINTERFACE;
    },

  // nsIMsgStatusFeedback implementation.
  showStatusString : function(statusText)
    {
      this.ensureStatusFields();
      if ( statusText == "" )
        statusText = defaultStatus;
      this.statusTextFld.label = statusText;
  },
  _startMeteors : function()
    {
      this.ensureStatusFields();

      this.meteorsSpinning = true;
      this.startTimeoutID = null;

      if (!this.progressMeterVisible)
      {
        this.progressMeterContainer.removeAttribute('collapsed'); 
        this.progressMeterVisible = true;
      }

      // Turn progress meter on.
      this.statusBar.setAttribute("mode","undetermined");

      // start the throbber
      if (this.throbber)
        this.throbber.setAttribute("busy", true);

      //turn on stop button and menu
      if (this.stopCmd)
    this.stopCmd.removeAttribute("disabled");
    },
  startMeteors : function()
    {
      this.pendingStartRequests++;
      // if we don't already have a start meteor timeout pending
      // and the meteors aren't spinning, then kick off a start
      if (!this.startTimeoutID && !this.meteorsSpinning && window.MsgStatusFeedback)
        this.startTimeoutID = setTimeout('window.MsgStatusFeedback._startMeteors();', 500);

      // since we are going to start up the throbber no sense in processing
      // a stop timeout...
      if (this.stopTimeoutID)
      {
        clearTimeout(this.stopTimeoutID);
        this.stopTimeoutID = null;
      }
  },
   _stopMeteors : function()
    {
      if(gTimelineEnabled){
        gTimelineService.stopTimer("FolderLoading");
        gTimelineService.markTimer("FolderLoading");
        gTimelineService.resetTimer("FolderLoading");
      }
      this.ensureStatusFields();
      var msg = gMessengerBundle.getString("documentDone");
      this.showStatusString(msg);
      defaultStatus = msg;
      
      // stop the throbber
      if (this.throbber)
        this.throbber.setAttribute("busy", false);

      // Turn progress meter off.
      this.statusBar.setAttribute("mode","normal");
      this.statusBar.value = 0;  // be sure to clear the progress bar
      this.statusBar.label = "";

      if (this.progressMeterVisible)
      {
        this.progressMeterContainer.collapsed = true;
        this.progressMeterVisible = false;
      }

      if (this.stopCmd)
        this.stopCmd.setAttribute("disabled", "true");

      this.meteorsSpinning = false;
      this.stopTimeoutID = null;
    },
   stopMeteors : function()
    {
      if (this.pendingStartRequests > 0)
        this.pendingStartRequests--;

      // if we are going to be starting the meteors, cancel the start
      if (this.pendingStartRequests == 0 && this.startTimeoutID)
      {
        clearTimeout(this.startTimeoutID);
        this.startTimeoutID = null;
      }

      // if we have no more pending starts and we don't have a stop timeout already in progress
      // AND the meteors are currently running then fire a stop timeout to shut them down.
      if (this.pendingStartRequests == 0 && !this.stopTimeoutID)
      {
        if (this.meteorsSpinning && window.MsgStatusFeedback)
          this.stopTimeoutID = setTimeout('window.MsgStatusFeedback._stopMeteors();', 500);
      }
  },
  showProgress : function(percentage)
    {
      this.ensureStatusFields();
      if (percentage >= 0)
      {
        this.statusBar.setAttribute("mode", "normal");
        this.statusBar.value = percentage;
        this.statusBar.label = Math.round(percentage) + "%";
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
    if (iid.equals(Components.interfaces.nsIMsgWindowCommands) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
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

function nsMessagePaneController()
{
}

nsMessagePaneController.prototype =
{
  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIMsgMessagePaneController) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },
  clearMsgPane: function()
  {
    if (gDBView)
      setTitleFromFolder(gDBView.msgFolder,null);
    else
      setTitleFromFolder(null,null);
    ClearMessagePane();
  }
}

function StopUrls()
{
  msgWindow.StopUrls();
}

function loadStartPage() {
    try {
        // collapse the junk bar
        SetUpJunkBar(null);
        SetUpRemoteContentBar(null);

        var startpageenabled = pref.getBoolPref("mailnews.start_page.enabled");

        if (startpageenabled) {
            var startpage = pref.getComplexValue("mailnews.start_page.url",
                                                 Components.interfaces.nsIPrefLocalizedString).data;
            if (startpage != "") {
                GetMessagePaneFrame().location = startpage;
                //dump("start message pane with: " + startpage + "\n");
                ClearMessageSelection();
            }
        }
    }
    catch (ex) {
        dump("Error loading start page.\n");
        return;
    }
}

// Display AccountCentral page when users clicks on the Account Folder.
// When AccountCentral page need to be shown, we need to hide
// the box containing threadPane, splitter and messagePane.
// Load iframe in the AccountCentral box with corresponding page
function ShowAccountCentral()
{
    try
    {
        var acctCentralPage = pref.getComplexValue("mailnews.account_central_page.url",
                                               Components.interfaces.nsIPrefLocalizedString).data;
        GetMessagePane().collapsed = true;
        document.getElementById("threadpane-splitter").collapsed = true;
        gSearchBox.collapsed = true;
        
        GetThreadTree().collapsed = true;
        document.getElementById("accountCentralBox").collapsed = false;

        window.frames["accountCentralPane"].location = acctCentralPage;
        
        if (!IsFolderPaneCollapsed())
            GetFolderTree().focus();        

        gAccountCentralLoaded = true;
    }
    catch (ex)
    {
        dump("Error loading AccountCentral page -> " + ex + "\n");
        return;
    }
}

// Display thread and message panes with splitter when user tries
// to read messages by clicking on msgfolders. Hide AccountCentral
// box and display message box.
function HideAccountCentral()
{
    try
    {
        window.frames["accountCentralPane"].location = "about:blank";
        document.getElementById("accountCentralBox").collapsed = true;
        GetThreadTree().collapsed = false;
        gSearchBox.collapsed = false;
        
        var threadPaneSplitter = document.getElementById("threadpane-splitter");
        threadPaneSplitter.collapsed = false;
        GetMessagePane().collapsed = threadPaneSplitter.getAttribute("state") == "collapsed";
        gAccountCentralLoaded = false;
    }
    catch (ex)
    {
        dump("Error hiding AccountCentral page -> " + ex + "\n");
        return;
    }
}

// Given the server, open the twisty and the set the selection
// on inbox of that server. 
// prompt if offline.
function OpenInboxForServer(server)
{
    try {
        HideAccountCentral();
        var inboxFolder = GetInboxFolder(server);
        SelectFolder(inboxFolder.URI);

        if(CheckOnline())	{
            if (server.type != "imap")
                GetMessagesForInboxOnServer(server);
        }
        else if (DoGetNewMailWhenOffline()) {
                GetMessagesForInboxOnServer(server);
        }
    }
    catch (ex) {
        dump("Error opening inbox for server -> " + ex + "\n");
        return;
    }
}

function GetSearchSession()
{
  if (("gSearchSession" in top) && gSearchSession)
    return gSearchSession;
  else
    return null;
}

function SetKeywords(aKeywords) 
{ 
  // we cache the last keywords.  
  // if there is no chagne, we do nothing.
  // most of the time, this will be the case.
  if (aKeywords == gLastKeywords)
    return;

  // these are the UI elements who care about keywords
  var elements = document.getElementsByAttribute("keywordrelated","true");
  var len = elements.length;
  for (var i=0; i<len; i++) {
    var element = elements[i];
    var originalclass = element.getAttribute("originalclass");

    // we use XBL for certain headers.
    // if the element has keywordrelated="true"
    // but no original class, it's an XBL widget
    // so to get the real element, use getAnonymousElementByAttribute()
    if (!originalclass) {
      element = document.getAnonymousElementByAttribute(element, "keywordrelated", "true");
      originalclass = element.getAttribute("originalclass");
    }

    if (aKeywords) {
      if (element.getAttribute("appendoriginalclass") == "true") {
        aKeywords += " " + originalclass;
      }
      element.setAttribute("class", aKeywords);
    }
    else {
      // if no keywords, reset class to the original class
      element.setAttribute("class", originalclass);
    }
  }

  // cache the keywords 
  gLastKeywords = aKeywords;
}
