# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jan Varga <varga@nixcorp.com>
#   Håkan Waara (hwaara@chello.se)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

//This file stores variables common to mail windows
var messengerContractID        = "@mozilla.org/messenger;1";
var statusFeedbackContractID   = "@mozilla.org/messenger/statusfeedback;1";
var mailSessionContractID      = "@mozilla.org/messenger/services/session;1";
var msgWindowContractID      = "@mozilla.org/messenger/msgwindow;1";

var messenger;
var pref;
var statusFeedback;
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
var unreadFoldersDSContractID = datasourceContractIDPrefix + "mailnewsunreadfolders";
var favoriteFoldersDSContractID = datasourceContractIDPrefix + "mailnewsfavefolders";
var recentFoldersDSContractID = datasourceContractIDPrefix + "mailnewsrecentfolders";
var accountManagerDataSource;
var folderDataSource;
var unreadFolderDataSource;
var favoriteFoldersDataSource;
var recentFoldersDataSource;

var accountCentralBox = null;
var gAccountCentralLoaded = true;
var gFakeAccountPageLoaded = false;
//End progress and Status variables

function OnMailWindowUnload()
{
  MailOfflineMgr.uninit();
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

  var msgDS;
  var viewDataSources = [accountManagerDataSource, folderDataSource, 
                        unreadFolderDataSource, favoriteFoldersDataSource,
                        recentFoldersDataSource];
                        
  for (index in viewDataSources)
  {
    msgDS = viewDataSources[index].QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
    msgDS.window = null;
  }

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

  pref = Components.classes["@mozilla.org/preferences-service;1"]
          .getService(Components.interfaces.nsIPrefBranch2);

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

  statusFeedback = Components.classes[statusFeedbackContractID].createInstance();
  statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);
  statusFeedback.setWrappedStatusFeedback(window.MsgStatusFeedback);

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
  unreadFolderDataSource = Components.classes[unreadFoldersDSContractID].getService();
  favoriteFoldersDataSource = Components.classes[favoriteFoldersDSContractID].getService();
  recentFoldersDataSource = Components.classes[recentFoldersDSContractID].getService();
}

function InitMsgWindow()
{
  msgWindow.windowCommands = new nsMsgWindowCommands();
  // set the domWindow before setting the status feedback and header sink objects
  msgWindow.domWindow = window; 
  msgWindow.statusFeedback = statusFeedback;
  msgWindow.msgHeaderSink = messageHeaderSink;
  mailSession.AddMsgWindow(msgWindow);
  getBrowser().docShell.allowAuth = false;
  msgWindow.rootDocShell.allowAuth = true; 
  msgWindow.rootDocShell.appType = Components.interfaces.nsIDocShell.APP_TYPE_MAIL;
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

  var msgDS;
  var viewDataSources = [accountManagerDataSource, folderDataSource, 
                        unreadFolderDataSource, favoriteFoldersDataSource,
                        recentFoldersDataSource];
  for (index in viewDataSources)
  {
    msgDS = viewDataSources[index].QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
    msgDS.window = msgWindow;
  }
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
  myDefaultStatus : null,
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
      if (status.length > 0)
        this.showStatusString(status);
    },
  setJSDefaultStatus : function(status)
    {
      if (status.length > 0)
      {
        this.myDefaultStatus = status;
        this.statusTextFld.label = status;
      }
    },
  setOverLink : function(link, context)
    {
      this.ensureStatusFields();
      this.statusTextFld.label = link;
    },
  QueryInterface : function(iid)
    {
      if (iid.equals(Components.interfaces.nsIMsgStatusFeedback) ||
          iid.equals(Components.interfaces.nsIXULBrowserWindow) ||
          iid.equals(Components.interfaces.nsISupportsWeakReference) || 
          iid.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_NOINTERFACE;
    },

  // nsIMsgStatusFeedback implementation.
  showStatusString : function(statusText)
    {
      this.ensureStatusFields();
      if ( !statusText.length )
        statusText = this.myDefaultStatus;
      else
        this.myDefaultStatus = "";
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
      this.showStatusString(defaultStatus);
      
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
  
  selectFolder: function(folderUri)
  {
    SelectFolder(folderUri);
  },
  
  selectMessage: function(messageUri)
  {
    SelectMessage(messageUri);
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

/** 
 * @returns the pref name to use for fetching the start page url. Every time the application version changes, 
 * return "mailnews.start_page.override_url". If this is the first time the application has been 
 * launched, return "mailnews.start_page.welcome_url". Otherwise return "mailnews.start_page.url".
 */
function startPageUrlPref()
{
  var prefForStartPageUrl = "mailnews.start_page.url";
  var savedVersion = null;
  try {
    savedVersion = pref.getCharPref("mailnews.start_page_override.mstone");
  } catch (ex) {}
  
  if (savedVersion != "ignore")
  {
    var currentPlatformVersion = Components.classes["@mozilla.org/xre/app-info;1"].
                                            getService(Components.interfaces.nsIXULAppInfo).platformVersion;
    pref.setCharPref("mailnews.start_page_override.mstone", currentPlatformVersion);
    // Use the welcome URL the first time we run
    if (!savedVersion)
      prefForStartPageUrl = "mailnews.start_page.welcome_url";
    else if (currentPlatformVersion != savedVersion)
      prefForStartPageUrl = "mailnews.start_page.override_url";
  }
  
  return prefForStartPageUrl;
}

function loadStartPage() 
{
  try
  {
    gMessageNotificationBar.clearMsgNotifications();
    
    var startpageenabled = pref.getBoolPref("mailnews.start_page.enabled");
    // only load the start page if we are online
    if (startpageenabled && MailOfflineMgr.isOnline())
    {
      var startpage = getFormattedRegionURL(startPageUrlPref());
      if (startpage)
      {
        GetMessagePaneFrame().location.href = startpage;
        ClearMessageSelection();
      }
    }
  }
  catch (ex)
  {
    dump("Error loading start page.\n");
    return;
  }
}

// When the ThreadPane is hidden via the displayDeck, we should collapse the
// elements that are only meaningful to the thread pane. When AccountCentral is
// shown via the displayDeck, we need to switch the displayDeck to show the
// accountCentralBox, and load the iframe in the AccountCentral box with
// corresponding page.
function ShowAccountCentral()
{
  try
  {
    document.getElementById("displayDeck").selectedPanel = accountCentralBox;
    var acctCentralPage = pref.getComplexValue("mailnews.account_central_page.url",
                                               Components.interfaces.nsIPrefLocalizedString).data;
    window.frames["accountCentralPane"].location.href = acctCentralPage;
  }
  catch (ex)
  {
    dump("Error loading AccountCentral page -> " + ex + "\n");
  }
}

function ShowingAccountCentral()
{
  if (!IsFolderPaneCollapsed())
    GetFolderTree().focus();        

  gAccountCentralLoaded = true;
}

function HidingAccountCentral()
{
  gAccountCentralLoaded = false;
}

function ShowThreadPane()
{
  document.getElementById("displayDeck").selectedPanel = 
    document.getElementById("threadPaneBox");
}

function ShowingThreadPane()
{
  var threadPaneSplitter = document.getElementById("threadpane-splitter");
  threadPaneSplitter.collapsed = false;
  GetMessagePane().collapsed = (threadPaneSplitter.getAttribute("state") == "collapsed");
  document.getElementById("key_toggleMessagePane").removeAttribute("disabled");
}

function HidingThreadPane()
{
  ClearThreadPane();
  GetUnreadCountElement().hidden = true;
  GetTotalCountElement().hidden = true;
  GetMessagePane().collapsed = true;
  document.getElementById("threadpane-splitter").collapsed = true;
  document.getElementById("key_toggleMessagePane").setAttribute("disabled", "true");
}

// the find toolbar needs a method called getBrowser
function getBrowser() 
{
  return document.getElementById("messagepane");
}

function ObserveDisplayDeckChange(event)
{
  var selectedPanel = document.getElementById("displayDeck").selectedPanel;
  var nowSelected = selectedPanel ? selectedPanel.id : "";

  if (nowSelected == "threadPaneBox")
    ShowingThreadPane();
  else
    HidingThreadPane();
  
  if (nowSelected == "accountCentralBox")
    ShowingAccountCentral();
  else
    HidingAccountCentral();
}

// Given the server, open the twisty and the set the selection
// on inbox of that server. 
// prompt if offline.
function OpenInboxForServer(server)
{
  try {
    ShowThreadPane();
    var inboxFolder = GetInboxFolder(server);
    SelectFolder(inboxFolder.URI);

    if (MailOfflineMgr.isOnline() || MailOfflineMgr.getNewMail())	{
      if (server.type != "imap")
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
