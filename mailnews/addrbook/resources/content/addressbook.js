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
 * The Original Code is Mozilla Addressbook.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1999-2001
 * the Initial Developer. All Rights Reserved.
 *
 * Original Author:
 *   Paul Hangas <hangas@netscape.com>
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
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

var cvPrefs = 0;
var addressbook = 0;
var gAddressBookBundle;
var gSearchTimer = null;
var gStatusText = null;
var gQueryURIFormat = null;
var gSearchInput;
var gPrintSettings = null;
var gDirTree;
var gSearchBox;
var gCardViewBox;
var gCardViewBoxEmail1;

// Constants that correspond to choices
// in Address Book->View -->Show Name as
const kDisplayName = 0;
const kLastNameFirst = 1;
const kFirstNameFirst = 2;
const kLDAPDirectory = 0; // defined in nsDirPrefs.h
const kPABDirectory  = 2; // defined in nsDirPrefs.h

var gAddressBookAbListener = {
  onItemAdded: function(parentDir, item) {
    // will not be called
  },
  onItemRemoved: function(parentDir, item) {
    // will only be called when an addressbook is deleted
    try {
      var directory = item.QueryInterface(Components.interfaces.nsIAbDirectory);
      // check if the item being removed is the directory
      // that we are showing in the addressbook
      // if so, select the personal addressbook (it can't be removed)
      if (directory && directory == GetAbView().directory) {
        SelectFirstAddressBook();
      }
    }
    catch (ex) {
    }
  },
  onItemPropertyChanged: function(item, property, oldValue, newValue) {
    // will not be called
  }
};

function OnUnloadAddressBook()
{  
  var addrbookSession =
        Components.classes["@mozilla.org/addressbook/services/session;1"]
                  .getService(Components.interfaces.nsIAddrBookSession);
  addrbookSession.removeAddressBookListener(gAddressBookAbListener);

  RemovePrefObservers();
  CloseAbView();
}

var gAddressBookAbViewListener = {
  onSelectionChanged: function() {
    ResultsPaneSelectionChanged();
  },
  onCountChanged: function(total) {
    SetStatusText(total);
  }
};

function GetAbViewListener()
{
  return gAddressBookAbViewListener;
}

const kPrefMailAddrBookLastNameFirst = "mail.addr_book.lastnamefirst";

var gMailAddrBookLastNameFirstObserver = {
  observe: function(subject, topic, value) {
    if (topic == "nsPref:changed" && value == kPrefMailAddrBookLastNameFirst) {
      UpdateCardView();
    }
  }
}

function AddPrefObservers()
{
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefBranch = prefService.getBranch(null).QueryInterface(Components.interfaces.nsIPrefBranchInternal);
  prefBranch.addObserver(kPrefMailAddrBookLastNameFirst, gMailAddrBookLastNameFirstObserver, false);
}

function RemovePrefObservers()
{
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefBranch = prefService.getBranch(null).QueryInterface(Components.interfaces.nsIPrefBranchInternal);
  prefBranch.removeObserver(kPrefMailAddrBookLastNameFirst, gMailAddrBookLastNameFirstObserver);
}

function OnLoadAddressBook()
{
  gAddressBookBundle = document.getElementById("bundle_addressBook");
  gSearchInput = document.getElementById("searchInput");

  verifyAccounts(null); 	// this will do migration, if we need to.

  top.addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);

  InitCommonJS();

  UpgradeAddressBookResultsPaneUI("mailnews.ui.addressbook_results.version");

  //This migrates the LDAPServer Preferences from 4.x to mozilla format.
  try {
    Components.classes["@mozilla.org/ldapprefs-service;1"]
              .getService(Components.interfaces.nsILDAPPrefsService);
  } catch (ex) {dump ("ERROR: Cannot get the LDAP service\n" + ex + "\n");}

  GetCurrentPrefs();

  AddPrefObservers();

  // FIX ME - later we will be able to use onload from the overlay
  OnLoadCardView();

  SetupAbCommandUpdateHandlers();

  //workaround - add setTimeout to make sure dynamic overlays get loaded first
  setTimeout('OnLoadDirTree()', 0);

  // if the pref is locked disable the menuitem New->LDAP directory
  if (gPrefs.prefIsLocked("ldap_2.disable_button_add"))
    document.getElementById("addLDAP").setAttribute("disabled", "true");

  // add a listener, so we can switch directories if
  // the current directory is deleted
  var addrbookSession =
        Components.classes["@mozilla.org/addressbook/services/session;1"]
                  .getService(Components.interfaces.nsIAddrBookSession);
  // this listener only cares when a directory is removed
  addrbookSession.addAddressBookListener(gAddressBookAbListener, Components.interfaces.nsIAbListener.directoryRemoved);

  var dirTree = GetDirTree();
  dirTree.addEventListener("click",DirPaneClick,true);
}

function OnLoadDirTree() {
  var treeBuilder = dirTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
  treeBuilder.addObserver(abDirTreeObserver);

  SelectFirstAddressBook();
}

function GetCurrentPrefs()
{
	// prefs
	if ( cvPrefs == 0 )
		cvPrefs = new Object;

	cvPrefs.prefs = gPrefs;
	
	// check "Show Name As" menu item based on pref
	var menuitemID;
	switch (gPrefs.getIntPref("mail.addr_book.lastnamefirst"))
	{
		case kFirstNameFirst:
			menuitemID = 'firstLastCmd';
			break;
		case kLastNameFirst:
			menuitemID = 'lastFirstCmd';
			break;
		case kDisplayName:
		default:
			menuitemID = 'displayNameCmd';
			break;
	}

	var menuitem = top.document.getElementById(menuitemID);
	if ( menuitem )
		menuitem.setAttribute('checked', 'true');

  // initialize phonetic 
  var showPhoneticFields =
        gPrefs.getComplexValue("mail.addr_book.show_phonetic_fields", 
                               Components.interfaces.nsIPrefLocalizedString).data;
  // show phonetic fields if indicated by the pref
  if (showPhoneticFields == "true")
    document.getElementById("cmd_SortBy_PhoneticName")
            .setAttribute("hidden", "false");

}


function SetNameColumn(cmd)
{
	var prefValue;
	
	switch ( cmd )
	{
		case 'firstLastCmd':
			prefValue = kFirstNameFirst;
			break;
		case 'lastFirstCmd':
			prefValue = kLastNameFirst;
			break;
		case 'displayNameCmd':
			prefValue = kDisplayName;
			break;
	}
	
	cvPrefs.prefs.setIntPref("mail.addr_book.lastnamefirst", prefValue);
}

function CommandUpdate_AddressBook()
{
  goUpdateCommand('cmd_delete');
  goUpdateCommand('button_delete');
}

function ResultsPaneSelectionChanged()
{
  UpdateCardView();
}

function UpdateCardView()
{
  var cards = GetSelectedAbCards();

  // display the selected card, if exactly one card is selected.
  // either no cards, or more than one card is selected, clear the pane.
  if (cards.length == 1)
    OnClickedCard(cards[0])
  else 
    ClearCardViewPane();
}

function OnClickedCard(card)
{ 
  if (card) 
    DisplayCardViewPane(card);
  else
    ClearCardViewPane();
}

function AbClose()
{
  top.close();
}

function AbNewLDAPDirectory()
{
  window.openDialog("chrome://messenger/content/addressbook/pref-directory-add.xul", 
                    "", 
                    "chrome,modal=yes,resizable=no,centerscreen", 
                    null);
}

function AbNewAddressBook()
{
  var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].
      getService(Components.interfaces.nsIStringBundleService);
  var bundle = strBundleService.createBundle("chrome://messenger/locale/addressbook/addressBook.properties");
  var dialogTitle = bundle.GetStringFromName('newAddressBookTitle');

  var dialog = window.openDialog(
    "chrome://messenger/content/addressbook/abAddressBookNameDialog.xul", 
     "", "chrome,modal=yes,resizable=no,centerscreen", {title: dialogTitle, okCallback:AbOnCreateNewAddressBook});
}

function AbRenameAddressBook()
{
  var selectedABURI = GetSelectedDirectory();
 
  // the rdf service
  var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

  // the RDF resource URI for LDAPDirectory will be like: "moz-abmdbdirectory://abook-3.mab"
  var selectedABDirectory = RDF.GetResource(selectedABURI).QueryInterface(Components.interfaces.nsIAbDirectory);

  var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].
      getService(Components.interfaces.nsIStringBundleService);
  var bundle = strBundleService.createBundle("chrome://messenger/locale/addressbook/addressBook.properties");
  var dialogTitle = bundle.GetStringFromName('renameAddressBookTitle');

  // you can't rename the PAB or the CAB
  var canRename = (selectedABURI != kCollectedAddressbookURI && selectedABURI != kPersonalAddressbookURI);

  var dialog = window.openDialog(
    "chrome://messenger/content/addressbook/abAddressBookNameDialog.xul", 
     "", "chrome,modal=yes,resizable=no,centerscreen", {title: dialogTitle, canRename: canRename, name: selectedABDirectory.directoryProperties.description,
      okCallback:AbOnRenameAddressBook});
}

function AbOnCreateNewAddressBook(aName)
{
  var properties = Components.classes["@mozilla.org/addressbook/properties;1"].createInstance(Components.interfaces.nsIAbDirectoryProperties);
  properties.description = aName;
  properties.dirType = kPABDirectory;
  top.addressbook.newAddressBook(properties);
}

function AbOnRenameAddressBook(aName)
{
  // When the UI code for renaming addrbooks (bug #17230) is ready, just 
  // change 'properties.description' setting below and it should just work.
  // get select ab
  var selectedABURI = GetSelectedDirectory();
  //dump("In AbRenameAddressBook() selectedABURI=" + selectedABURI + "\n");

  // the rdf service
  var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
  // get the datasource for the addressdirectory
  var addressbookDS = RDF.GetDataSource("rdf:addressdirectory");

  // moz-abdirectory:// is the RDF root to get all types of addressbooks.
  var parentDir = RDF.GetResource("moz-abdirectory://").QueryInterface(Components.interfaces.nsIAbDirectory);

  // the RDF resource URI for LDAPDirectory will be like: "moz-abmdbdirectory://abook-3.mab"
  var selectedABDirectory = RDF.GetResource(selectedABURI).QueryInterface(Components.interfaces.nsIAbDirectory);

  // Copy existing dir type category id and mod time so they won't get reset.
  var oldProperties = selectedABDirectory.directoryProperties;

  // Create and fill in properties info
  var properties = Components.classes["@mozilla.org/addressbook/properties;1"].createInstance(Components.interfaces.nsIAbDirectoryProperties);
  properties.URI = selectedABURI;
  properties.dirType = oldProperties.dirType;
  properties.categoryId = oldProperties.categoryId;
  properties.syncTimeStamp = oldProperties.syncTimeStamp;
  properties.description = aName;

  // Now do the modification.
  addressbook.modifyAddressBook(addressbookDS, parentDir, selectedABDirectory, properties);
}

function GetPrintSettings()
{
  var prevPS = gPrintSettings;

  try {
    if (gPrintSettings == null) {
      var useGlobalPrintSettings = true;
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
      if (pref) {
        useGlobalPrintSettings = pref.getBoolPref("print.use_global_printsettings", false);
      }

      // I would rather be using nsIWebBrowserPrint API
      // but I really don't have a document at this point
      var printSettingsService = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                                           .getService(Components.interfaces.nsIPrintSettingsService);
      if (useGlobalPrintSettings) {
        gPrintSettings = printSettingsService.globalPrintSettings;
      } else {
        gPrintSettings = printSettingsService.CreatePrintSettings();
      }
    }
  } catch (e) {
    dump("GetPrintSettings "+e);
  }

  return gPrintSettings;
}

function AbPrintCardInternal(doPrintPreview, msgType)
{
  var selectedItems = GetSelectedAbCards();
  var numSelected = selectedItems.length;

  if (!numSelected)
    return;

  var addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);
  var uri = GetSelectedDirectory();
  if (!uri)
    return;

   var statusFeedback;
   statusFeedback = Components.classes["@mozilla.org/messenger/statusfeedback;1"].createInstance();
   statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

   var selectionArray = new Array(numSelected);

   var totalCard = 0;

   for (var i = 0; i < numSelected; i++)
   {
     var card = selectedItems[i];
     var printCardUrl = CreatePrintCardUrl(card);
     if (printCardUrl)
     {
        selectionArray[totalCard++] = printCardUrl;
     }
  }

  if (!gPrintSettings)
  {
    gPrintSettings = GetPrintSettings();
  }

  printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul",
                                         "",
                                         "chrome,dialog=no,all",
                                          totalCard, selectionArray, statusFeedback, 
                                          gPrintSettings, doPrintPreview, msgType);

  return;
}

function AbPrintCard()
{
  AbPrintCardInternal(false, Components.interfaces.nsIMsgPrintEngine.MNAB_PRINT_AB_CARD);
}

function AbPrintPreviewCard()
{
  AbPrintCardInternal(true, Components.interfaces.nsIMsgPrintEngine.MNAB_PRINTPREVIEW_AB_CARD);
}

function CreatePrintCardUrl(card)
{
  var url = "data:text/xml;base64," + card.convertToBase64EncodedXML();
  return url;
}

function AbPrintAddressBookInternal(doPrintPreview, msgType)
{
  var addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);
  var uri = GetSelectedDirectory();
  if (!uri)
    return;

  var statusFeedback;
	statusFeedback = Components.classes["@mozilla.org/messenger/statusfeedback;1"].createInstance();
	statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

  /*
    turn "moz-abmdbdirectory://abook.mab" into
    "addbook://moz-abmdbdirectory/abook.mab?action=print"
   */

  var abURIArr = uri.split("://");
  var printUrl = "addbook://" + abURIArr[0] + "/" + abURIArr[1] + "?action=print"

  if (!gPrintSettings) {
    gPrintSettings = GetPrintSettings();
  }

	printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul",
										"",
										"chrome,dialog=no,all",
										1, [printUrl], statusFeedback, gPrintSettings, doPrintPreview, msgType);

	return;
}

function AbPrintAddressBook()
{
  AbPrintAddressBookInternal(false, Components.interfaces.nsIMsgPrintEngine.MNAB_PRINT_ADDRBOOK);
}

function AbPrintPreviewAddressBook()
{
  AbPrintAddressBookInternal(true, Components.interfaces.nsIMsgPrintEngine.MNAB_PRINTPREVIEW_ADDRBOOK);
}

function AbExport()
{
  try {
    var selectedABURI = GetSelectedDirectory();
    if (!selectedABURI) return;
    
    var directory = GetDirectoryFromURI(selectedABURI);
    addressbook.exportAddressBook(window, directory);
  }
  catch (ex) {
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);

    if (promptService) {
      var message;
      switch (ex.result) {
        case Components.results.NS_ERROR_FILE_ACCESS_DENIED:
          message = gAddressBookBundle.getString("failedToExportMessageFileAccessDenied");
          break;
        case Components.results.NS_ERROR_FILE_NO_DEVICE_SPACE:
          message = gAddressBookBundle.getString("failedToExportMessageNoDeviceSpace");
          break;
        default:
          message = ex.message;
          break;
      }

      promptService.alert(window,
        gAddressBookBundle.getString("failedToExportTitle"), 
        message);
    }
  }
}

function AbDeleteDirectory()
{
  var selectedABURI = GetSelectedDirectory();
  if (!selectedABURI)
    return;

  var parentArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  if (!parentArray) 
    return; 

  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
 
  var parentRow = dirTree.view.getParentIndex(dirTree.currentIndex);
  var parentId = (parentRow == -1) ? "moz-abdirectory://" : dirTree.builderView.getResourceAtIndex(parentRow).Value;
  var parentDir = GetDirectoryFromURI(parentId);
  parentArray.AppendElement(parentDir);
    
  var directory = GetDirectoryFromURI(selectedABURI);
  var confirmDeleteMessage = gAddressBookBundle.getString(directory.isMailList ? "confirmDeleteMailingList" : "confirmDeleteAddressbook");

  if (!promptService.confirm(window, null, confirmDeleteMessage))
    return;

  var resourceArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  var selectedABResource = GetDirectoryFromURI(selectedABURI).QueryInterface(Components.interfaces.nsIRDFResource);

  resourceArray.AppendElement(selectedABResource);

  top.addressbook.deleteAddressBooks(dirTree.database, parentArray, resourceArray);
  SelectFirstAddressBook();
}

function SetStatusText(total)
{
  if (!gStatusText)
    gStatusText = document.getElementById('statusText');

  try {
    var statusText;

    if (gSearchInput.value) {
      if (total == 0)
        statusText = gAddressBookBundle.getString("noMatchFound");
      else
      {
        if (total == 1)
          statusText = gAddressBookBundle.getString("matchFound");
        else  
          statusText = gAddressBookBundle.getFormattedString("matchesFound", [total]);
      }
    } 
    else
      statusText = gAddressBookBundle.getFormattedString("totalCardStatus", [gAbView.directory.dirName, total]);   

    gStatusText.setAttribute("label", statusText);
  }
  catch(ex) {
    dump("failed to set status text:  " + ex + "\n");
  }
}

function AbResultsPaneDoubleClick(card)
{
  AbEditCard(card);
}

function onAdvancedAbSearch()
{
  var selectedABURI = GetSelectedDirectory();
  if (!selectedABURI) return;

  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].
                                 getService(Components.interfaces.nsIWindowMediator);
  var existingSearchWindow = windowManager.getMostRecentWindow("mailnews:absearch");
  if (existingSearchWindow)
    existingSearchWindow.focus();
  else
    window.openDialog("chrome://messenger/content/ABSearchDialog.xul", "", 
                      "chrome,resizable,status,centerscreen,dialog=no", 
                      {directory: selectedABURI});
}

function onEnterInSearchBar()
{
  ClearCardViewPane();  

  if (!gQueryURIFormat)
    gQueryURIFormat = gPrefs.getComplexValue("mail.addr_book.quicksearchquery.format", 
                                              Components.interfaces.nsIPrefLocalizedString).data;

  var searchURI = GetSelectedDirectory();
  if (!searchURI) return;

  var sortColumn = gAbView.sortColumn;
  var sortDirection = gAbView.sortDirection;

  /*
   XXX todo, handle the case where the LDAP url
   already has a query, like 
   moz-abldapdirectory://nsdirectory.netscape.com:389/ou=People,dc=netscape,dc=com?(or(Department,=,Applications))
  */
  if (gSearchInput.value != "") {
    // replace all instances of @V with the escaped version
    // of what the user typed in the quick search text input
    searchURI += gQueryURIFormat.replace(/@V/g, encodeURIComponent(gSearchInput.value));
  }

  SetAbView(searchURI, gSearchInput.value != "", sortColumn, sortDirection);
  
  // XXX todo 
  // this works for synchronous searches of local addressbooks, 
  // but not for LDAP searches
  SelectFirstCard();
}

function SwitchPaneFocus(event)
{
  var focusedElement    = WhichPaneHasFocus();
  var cardViewBox       = GetCardViewBox();
  var cardViewBoxEmail1 = GetCardViewBoxEmail1();
  var searchBox         = GetSearchBox();
  var dirTree           = GetDirTree();
  var searchInput       = GetSearchInput();

  if (event && event.shiftKey)
  {
    if (focusedElement == gAbResultsTree && searchBox.getAttribute('hidden') != 'true')
      searchInput.focus();
    else if ((focusedElement == gAbResultsTree || focusedElement == searchBox) && !IsDirPaneCollapsed())
      dirTree.focus();
    else if (focusedElement != cardViewBox && !IsCardViewAndAbResultsPaneSplitterCollapsed())
    {
      if(cardViewBoxEmail1)
        cardViewBoxEmail1.focus();
      else
        cardViewBox.focus();    
    }
    else 
      gAbResultsTree.focus();
  }
  else
  {
    if (focusedElement == searchBox)
      gAbResultsTree.focus();
    else if (focusedElement == gAbResultsTree && !IsCardViewAndAbResultsPaneSplitterCollapsed())
    {
      if(cardViewBoxEmail1)
        cardViewBoxEmail1.focus();
      else
        cardViewBox.focus();    
    }
    else if (focusedElement != dirTree && !IsDirPaneCollapsed())
      dirTree.focus();
    else if (searchBox.getAttribute('hidden') != 'true')
      searchInput.focus();
    else
      gAbResultsTree.focus();
  }
}

function WhichPaneHasFocus()
{
  var cardViewBox       = GetCardViewBox();
  var searchBox         = GetSearchBox();
  var dirTree           = GetDirTree();
    
  var currentNode = top.document.commandDispatcher.focusedElement;
  while (currentNode)
  {
    var nodeId = currentNode.getAttribute('id');

    if(currentNode == gAbResultsTree ||
       currentNode == cardViewBox ||
       currentNode == searchBox ||
       currentNode == dirTree)
      return currentNode;

    currentNode = currentNode.parentNode;
  }

  return null;
}

function GetDirTree()
{
  if (!gDirTree)
    gDirTree = document.getElementById('dirTree');
  return gDirTree;
}

function GetSearchInput()
{
  if (!gSearchInput)
    gSearchInput = document.getElementById('searchInput');
  return gSearchInput;
}

function GetSearchBox()
{
  if (!gSearchBox)
    gSearchBox = document.getElementById('searchBox');
  return gSearchBox;
}

function GetCardViewBox()
{
  if (!gCardViewBox)
    gCardViewBox = document.getElementById('CardViewBox');
  return gCardViewBox;
}

function GetCardViewBoxEmail1()
{
  if (!gCardViewBoxEmail1)
  {
    try {
      gCardViewBoxEmail1 = document.getElementById('cvEmail1');
    }
    catch (ex) {
      gCardViewBoxEmail1 = null;
    }
  }
  return gCardViewBoxEmail1;
}

function IsDirPaneCollapsed()
{
  var dirPaneBox = GetDirTree().parentNode;
  return dirPaneBox.getAttribute("collapsed") == "true" ||
         dirPaneBox.getAttribute("hidden") == "true";
}

function IsCardViewAndAbResultsPaneSplitterCollapsed()
{
  var cardViewBox = document.getElementById('CardViewOuterBox');
  try {
    return (cardViewBox.getAttribute("collapsed") == "true");
  }
  catch (ex) {
    return false;
  }
}

function LaunchUrl(url)
{
  var messenger = Components.classes["@mozilla.org/messenger;1"].createInstance(Components.interfaces.nsIMessenger);
  messenger.SetWindow(window, null);
  messenger.OpenURL(url);
}

function AbIMSelected()
{
  var cards = GetSelectedAbCards();
  var count = cards.length;

  var screennames;
  var screennameCount = 0;

  for (var i=0;i<count;i++) {
    var screenname = cards[i].aimScreenName;
    if (screenname) {
      if (screennameCount == 0)
        screennames = screenname;
      else
        screennames += "," + screenname;

      screennameCount++
    }
  }

  var url = "aim:";

  if (screennameCount == 0)
    url += "goim";
  else if (screennameCount == 1)
    url += "goim?screenname=" + screennames;
  else {
    url += "SendChatInvite?listofscreennames=" + screennames;
    url += "&message=" + gAddressBookBundle.getString("joinMeInThisChat");
  }

  LaunchUrl(url);
}
