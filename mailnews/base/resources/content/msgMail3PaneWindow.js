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

/* This is where functions related to the 3 pane window are kept */

/* globals for a particular window */
var messengerProgID        = "component://netscape/messenger";
var statusFeedbackProgID   = "component://netscape/messenger/statusfeedback";
var messageViewProgID      = "component://netscape/messenger/messageview";
var mailSessionProgID      = "component://netscape/messenger/services/session";
var accountManagerProgID   = "component://netscape/messenger/account-manager";
var messengerMigratorProgID   = "component://netscape/messenger/migrator";
var prefProgID             = "component://netscape/preferences";
var msgWindowProgID		   = "component://netscape/messenger/msgwindow";

var datasourceProgIDPrefix = "component://netscape/rdf/datasource?name=";
var accountManagerDSProgID = datasourceProgIDPrefix + "msgaccountmanager";
var folderDSProgID         = datasourceProgIDPrefix + "mailnewsfolders";
var messageDSProgID        = datasourceProgIDPrefix + "mailnewsmessages";

var messenger;
var accountManagerDataSource;
var folderDataSource;
var messageDataSource;
var pref;
var statusFeedback;
var messageView;
var msgWindow;

var msgComposeService;
var mailSession;
var accountManager;
var RDF;
var showPerformance;
var msgNavigationService;

var msgComposeType;
var msgComposeFormat;
var Bundle;
var BrandBundle;

var gFolderTree;
var gThreadTree;
var gThreadAndMessagePaneSplitter = null;
var gUnreadCount = null;
var gTotalCount = null;

var gCurrentLoadingFolderURI;
var gCurrentFolderToReroot;
var gCurrentLoadingFolderIsThreaded = false;
var gCurrentLoadingFolderSortID ="";
var gCurrentLoadingFolderSortDirection = null;

var gCurrentDisplayedMessage = null;
var gNextMessageAfterDelete = null;

var gActiveThreadPaneSortColumn = "";

// the folderListener object
var folderListener = {
    OnItemAdded: function(parentItem, item, view) {},

	OnItemRemoved: function(parentItem, item, view){},

	OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

	OnItemIntPropertyChanged: function(item, property, oldValue, newValue)
	{
		var currentLoadedFolder = GetThreadTreeFolder();
		var currentURI = currentLoadedFolder.getAttribute('ref');

		//if we don't have a folder loaded, don't bother.
		if(currentURI)
		{
			if(property.GetUnicode() == "TotalMessages" || property.GetUnicode() == "TotalUnreadMessages")
			{
				folder = item.QueryInterface(Components.interfaces.nsIMsgFolder);
				if(folder)
				{
					var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
					if(folderResource)
					{
						var folderURI = folderResource.Value;
						if(currentURI == folderURI)
						{
							UpdateStatusMessageCounts(folder);
						}
					}


				}



			}
		}
	},

	OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue){},
	OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {},

	OnFolderLoaded: function (folder)
	{
		if(folder)
		{
			var resource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
			if(resource)
			{
				var uri = resource.Value;
				dump('In OnFolderLoaded for ' + uri);
				dump('\n');
				if(uri == gCurrentFolderToReroot)
				{
					gCurrentFolderToReroot="";
					var msgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
					if(msgFolder)
					{
						msgFolder.endFolderLoading();
						dump("before reroot in OnFolderLoaded\n");
						RerootFolder(uri, msgFolder, gCurrentLoadingFolderIsThreaded, gCurrentLoadingFolderSortID, gCurrentLoadingFolderSortDirection);
						gCurrentLoadingFolderIsThreaded = false;
						gCurrentLoadingFolderSortID = "";
						gCurrentLoadingFolderSortDirection = null;
					}
				}
				if(uri == gCurrentLoadingFolderURI)
				{
				  gCurrentLoadingFolderURI = "";
				  //Now let's select the first new message if there is one
				  var beforeScrollToNew = new Date();
				  msgNavigationService.EnsureDocumentIsLoaded(document);

				  ScrollToFirstNewMessage();
				  var afterScrollToNew = new Date();
				  var timeToScroll = (afterScrollToNew.getTime() - beforeScrollToNew.getTime())/1000;


				  var afterFolderLoadTime = new Date();
				  var timeToLoad = (afterFolderLoadTime.getTime() - gBeforeFolderLoadTime.getTime())/1000;
				  if(showPerformance)
				  {
					  dump("Time to load " + uri + " is " +  timeToLoad + " seconds\n");
				  	  dump("of which scrolling to new is" + timeToScroll + "seconds\n");
				  }
				}
			}

		}
	},
	OnDeleteOrMoveMessagesCompleted :function(folder)
	{
		dump("In OnDeleteOrMoveMessagesCompleted\n");
		if(IsCurrentLoadedFolder(folder))
		{
			msgNavigationService.EnsureDocumentIsLoaded(document);

			dump("next message uri is " + gNextMessageAfterDelete + "\n");
			if(gNextMessageAfterDelete)
			{
				var nextMessage = document.getElementById(gNextMessageAfterDelete);
				if(!nextMessage)
					dump("No next message after delete\n");
				SelectNextMessage(nextMessage);
				var threadTree = GetThreadTree();
				if(threadTree)
					threadTree.ensureElementIsVisible(nextMessage);
				else
					dump("No thread tree\n");
				gNextMessageAfterDelete = null;
			}
		}
	}
}

function IsCurrentLoadedFolder(folder)
{
	msgfolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
	if(msgfolder)
	{
		dump("IsCurrentLoadedFolder: has msgFolder\n");
		var folderResource = msgfolder.QueryInterface(Components.interfaces.nsIRDFResource);
		if(folderResource)
		{
			dump("IsCurrentLoadedFolder: has folderResource\n");
			var folderURI = folderResource.Value;
			var currentLoadedFolder = GetThreadTreeFolder();
			var currentURI = currentLoadedFolder.getAttribute('ref');
			dump("IsCurrentLoadedFolder: folderURI = " + folderURI + "\n");
			dump("IsCurrentLoadedFolder: currentURI = " + currentURI + "\n");
			return(currentURI == folderURI);
		}
	}

	return false;
}

/* Functions related to startup */
function OnLoadMessenger()
{
  var beforeLoadMessenger = new Date();

  CreateGlobals();
  verifyAccounts();
    
  loadStartPage();
	InitMsgWindow();

	messenger.SetWindow(window, msgWindow);

	AddDataSources();
	InitPanes();


  AddToSession();
  //need to add to session before trying to load start folder otherwise listeners aren't
  //set up correctly.
  dump('Before load start folder\n');
  setTimeout("loadStartFolder();", 0);

  // FIX ME - later we will be able to use onload from the overlay
  OnLoadMsgHeaderPane();

	var id = null;
	var headerchoice = null;

	try {
		headerchoice = pref.GetIntPref("mail.show_headers");
	}
	catch (ex) {
		dump("failed to get the header pref\n");
	}

	switch (headerchoice) {
		case 2:	
			id = "viewallheaders";
			break;
		case 0:
			id = "viewbriefheaders";
			break;
		case 1:	
			id = "viewnormalheaders";
			break;
		default:
			id = "viewnormalheaders";
			break;
	}

	var menuitem = document.getElementById(id);

	try {
		// not working right yet.  see bug #??????
		// menuitem.setAttribute("checked", "true"); 
	}
	catch (ex) {
		dump("failed to set the view headers menu item\n");
	}

	var afterLoadMessenger = new Date();

	var timeToLoad = (afterLoadMessenger.getTime() - beforeLoadMessenger.getTime())/1000;
	if(showPerformance)
	{
	  dump("Time in OnLoadMessger is " +  timeToLoad + " seconds\n");
	}

}

function OnUnloadMessenger()
{
	dump("\nOnUnload from XUL\nClean up ...\n");
	var mailSession = Components.classes[mailSessionProgID].getService();
	if(mailSession)
	{
		mailSession = mailSession.QueryInterface(Components.interfaces.nsIMsgMailSession);
		if(mailSession)
		{
			mailSession.RemoveFolderListener(folderListener);
			mailSession.RemoveMsgWindow(msgWindow);
			messenger.SetWindow(null, null);
		}
	}
	
	var msgDS = folderDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = null;

	msgDS = messageDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = null;

	msgDS = accountManagerDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = null;


  	msgWindow.closeWindow();
}


function CreateGlobals()
{
	// get the messenger instance
	messenger = Components.classes[messengerProgID].createInstance();
	messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);

	//Create datasources
	accountManagerDataSource = Components.classes[accountManagerDSProgID].createInstance();
	folderDataSource         = Components.classes[folderDSProgID].createInstance();
	messageDataSource        = Components.classes[messageDSProgID].createInstance();

	pref = Components.classes[prefProgID].getService(Components.interfaces.nsIPref);

	//Create windows status feedback
	statusFeedback           = Components.classes[statusFeedbackProgID].createInstance();
	statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

	//Create message view object
	messageView = Components.classes[messageViewProgID].createInstance();
	messageView = messageView.QueryInterface(Components.interfaces.nsIMessageView);

	//Create message window object
	msgWindow = Components.classes[msgWindowProgID].createInstance();
	msgWindow = msgWindow.QueryInterface(Components.interfaces.nsIMsgWindow);

	msgComposeService = Components.classes['component://netscape/messengercompose'].getService();
	msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

	mailSession = Components.classes["component://netscape/messenger/services/session"].getService(Components.interfaces.nsIMsgMailSession); 

	accountManager = Components.classes["component://netscape/messenger/account-manager"].getService(Components.interfaces.nsIMsgAccountManager);

	RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

	showPerformance = pref.GetBoolPref('mail.showMessengerPerformance');

	msgNavigationService = Components.classes['component://netscape/messenger/msgviewnavigationservice'].getService();
	msgNavigationService= msgNavigationService.QueryInterface(Components.interfaces.nsIMsgViewNavigationService);

	msgComposeType = Components.interfaces.nsIMsgCompType;
	msgComposeFormat = Components.interfaces.nsIMsgCompFormat;
	Bundle = srGetStrBundle("chrome://messenger/locale/messenger.properties");
    BrandBundle = srGetStrBundle("chrome://global/locale/brand.properties");

}

function verifyAccounts() {
    var openWizard = false;
    var prefillAccount;
    
    try {
        var am = Components.classes[accountManagerProgID].getService(Components.interfaces.nsIMsgAccountManager);

        var accounts = am.accounts;

        // as long as we have some accounts, we're fine.
        var accountCount = accounts.Count();
        if (accountCount > 0) {
            prefillAccount = getFirstInvalidAccount(accounts);
            dump("prefillAccount = " + prefillAccount + "\n");
        } else {
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


function loadStartPage() {
    try {
		startpageenabled= pref.GetBoolPref("mailnews.start_page.enabled");
        
		if (startpageenabled) {
			startpage = pref.CopyCharPref("mailnews.start_page.url");
            if (startpage != "") {
                window.frames["messagepane"].location = startpage;
                dump("start message pane with: " + startpage + "\n");
            }
        }
	}
    catch (ex) {
        dump("Error loading start page.\n");
        return;
    }
}

function loadStartFolder()
{
	//First get default account
	try
	{
		var defaultAccount = accountManager.defaultAccount;

		var server = defaultAccount.incomingServer;
		var rootFolder = server.RootFolder;
		var rootMsgFolder = rootFolder.QueryInterface(Components.interfaces.nsIMsgFolder);

		//now find Inbox
		var outNumFolders = new Object();
		dump('Before getting inbox\n');
		var inboxFolder = rootMsgFolder.getFoldersWithFlag(0x1000, 1, outNumFolders); 
		if(!inboxFolder) return;
		dump('We have an inbox\n');

		var resource = inboxFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		var inboxURI = resource.Value;

		dump('InboxURI = ' + inboxURI + '\n');
		//first, let's see if it's already in the dom.  This will make life easier.
		//We need to make sure content is built by this time
		msgNavigationService.EnsureDocumentIsLoaded(document);

		var inbox = document.getElementById(inboxURI);

		//if it's not here we will have to make sure it's open.
		if(!inbox)
		{
			dump('There isnt an inbox in the tree yet\n');

		}

		var folderTree= GetFolderTree();
		ChangeSelection(folderTree, inbox);
	}
	catch(ex)
	{
		dump(ex);
		dump('Exception in LoadStartFolder caused by no default account.  We know about this\n');
	}

}

function AddToSession()
{
    try {
        var mailSession = Components.classes[mailSessionProgID].getService(Components.interfaces.nsIMsgMailSession);
        
        mailSession.AddFolderListener(folderListener);
        mailSession.AddMsgWindow(msgWindow);
	} catch (ex) {
        dump("Error adding to session\n");
    }
}

function InitMsgWindow()
{
	msgWindow.statusFeedback = statusFeedback;
	msgWindow.messageView = messageView;
	msgWindow.msgHeaderSink = messageHeaderSink;
  msgWindow.SetDOMWindow(window);
}

function AddDataSources()
{

	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	//to move menu item
	SetupMoveCopyMenus('moveMenu', accountManagerDataSource, folderDataSource);

	//to copy menu item
	SetupMoveCopyMenus('copyMenu', accountManagerDataSource, folderDataSource);

	//To threadpane move context menu
	SetupMoveCopyMenus('threadPaneContext-moveMenu', accountManagerDataSource, folderDataSource);

	//To threadpane copy content menu
	SetupMoveCopyMenus('threadPaneContext-copyMenu', accountManagerDataSource, folderDataSource);

	//To FileButton menu
	SetupMoveCopyMenus('FileButtonMenu', accountManagerDataSource, folderDataSource);
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
	dump("SetupMoveCopyMenus for " + menuid + "\n");
	var menu = document.getElementById(menuid);
	if(menu)
	{
		menu.database.AddDataSource(accountManagerDataSource);
		menu.database.AddDataSource(folderDataSource);
		menu.setAttribute('ref', 'msgaccounts:/');
	}
}

function InitPanes()
{
	var threadTree = GetThreadTree();
	if(threadTree);
		OnLoadThreadPane(threadTree);

	var folderTree = GetFolderTree();
	if(folderTree)
		OnLoadFolderPane(folderTree);
		
	SetupCommandUpdateHandlers();
}

function OnLoadFolderPane(folderTree)
{
	dump('In onLoadfolderPane\n');
    gFolderTree = folderTree;
	SortFolderPane('FolderColumn', 'http://home.netscape.com/NC-rdf#FolderTreeName');

	//Add folderDataSource and accountManagerDataSource to folderPane
	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	var database = folderTree.database;

	database.AddDataSource(accountManagerDataSource);
    database.AddDataSource(folderDataSource);
	folderTree.setAttribute('ref', 'msgaccounts:/');
}

function OnLoadThreadPane(threadTree)
{
    gThreadTree = threadTree;
	//Sort by date by default
	// add folderSource to thread pane
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	threadTree.database.AddDataSource(folderDataSource);

	//Add message data source
	messageDataSource = messageDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	threadTree.database.AddDataSource(messageDataSource);

    //FIX ME: Tempory patch for bug 24182
    //ShowThreads(false);
	setTimeout("ShowThreads(false);", 0);
}

/* Functions for accessing particular parts of the window*/
function GetFolderTree()
{
    if (gFolderTree) return gFolderTree;
    
	var folderTree = document.getElementById('folderTree');
    gFolderTree = folderTree;
	return folderTree;
}

function FindInSidebar(currentWindow, id)
{
	var item = currentWindow.document.getElementById(id);
	if(item)
		return item;

	for(var i = 0; i < currentWindow.frames.length; i++)
	{
		var frameItem = FindInSidebar(currentWindow.frames[i], id);
		if(frameItem)
			return frameItem;
	}
}

function GetThreadTree()
{
    if (gThreadTree) return gThreadTree;
	var threadTree = document.getElementById('threadTree');
	if(!threadTree)
		dump('thread tree is null\n');
    gThreadTree = threadTree;
	return threadTree;
}

function GetThreadTreeFolder()
{
  var tree = GetThreadTree();
  return tree;
}

function GetThreadAndMessagePaneSplitter()
{
	if(gThreadAndMessagePaneSplitter) return gThreadAndMessagePaneSplitter;
	var splitter = document.getElementById('gray_horizontal_splitter');
	gThreadAndMessagePaneSplitter = splitter;
	return splitter;
}

function GetUnreadCountElement()
{
	if(gUnreadCount) return gUnreadCount;
	var unreadCount = document.getElementById('unreadMessageCount');
	gUnreadCount = unreadCount;
	return unreadCount;
}
function GetTotalCountElement()
{
	if(gTotalCount) return gTotalCount;
	var totalCount = document.getElementById('totalMessageCount');
	gTotalCount = totalCount;
	return totalCount;
}
function IsThreadAndMessagePaneSplitterCollapsed()
{
	var splitter = GetThreadAndMessagePaneSplitter();
	if(splitter)
	{
		var state  = splitter.getAttribute('state');
		return (state == "collapsed");
	}
	else
		return false;
}

function FindMessenger()
{
  return messenger;
}

function RefreshThreadTreeView()
{
	var selection = SaveThreadPaneSelection();

	var currentFolder = GetThreadTreeFolder();  
	var currentFolderID = currentFolder.getAttribute('ref');
	ClearThreadTreeSelection();
	currentFolder.setAttribute('ref', currentFolderID);

	RestoreThreadPaneSelection(selection);
}

function ClearThreadTreeSelection()
{
	var tree = GetThreadTree();
	if(tree)
	{
		tree.clearItemSelection();
	}

}

function ClearMessagePane()
{
	gCurrentDisplayedMessage = null;
    if (window.frames["messagepane"].location != "about:blank")
        window.frames["messagepane"].location = "about:blank";
    // hide the message header view AND the message pane...
    HideMessageHeaderPane();
}

function StopUrls()
{
	msgWindow.StopUrls();
}

function GetSelectedFolder()
{
	var tree = GetFolderTree();
	var selection = tree.selectedItems;
	if(selection.length > 0)
		return selection[0];
	else
		return null;

}

function ThreadPaneOnClick(event)
{
    var targetclass = event.target.getAttribute('class');
    debug('targetclass = ' + targetclass + '\n');

    if (targetclass == 'twisty') {
        // The twisty is nested three below the treeitem:
        // <treeitem>
        //   <treerow>
        //     <treecell>
        //         <titledbutton class="twisty"> <!-- anonymous -->
        var treeitem = event.target.parentNode.parentNode.parentNode;
		var open = treeitem.getAttribute('open');
		if(open == "true")
		{
			//open all of the children of the treeitem
			msgNavigationService.OpenTreeitemAndDescendants(treeitem);
		}
		dump('clicked on a twisty\n');
    }
	else if(event.clickCount == 2)
	{
		ThreadPaneDoubleClick(event.target.parentNode.parentNode);
	}
}

function ThreadPaneDoubleClick(treeitem)
{
	if(IsSpecialFolderSelected("Drafts"))
	{
		ComposeMessage(msgComposeType.Draft, msgComposeFormat.Default);
	}
	else if(IsSpecialFolderSelected("Templates"))
	{
		ComposeMessage(msgComposeType.Template, msgComposeFormat.Default);
	}
}

function ChangeSelection(tree, newSelection)
{
	if(newSelection)
	{
		tree.clearItemSelection();
		tree.clearCellSelection();
		tree.selectItem(newSelection);
		tree.ensureElementIsVisible(newSelection);
	}
}

function SetActiveThreadPaneSortColumn(column)
{
	gActiveThreadPaneSortColumn = column;
}

function GetActiveThreadPaneSortColumn()
{
	return gActiveThreadPaneSortColumn;
}

function ClearActiveThreadPaneSortColumn()
{
	var activeColumn = document.getElementById(gActiveThreadPaneSortColumn);
	if(activeColumn)
	{
		activeColumn.removeAttribute("sortActive");
		activeColumn = "";
	}

}
