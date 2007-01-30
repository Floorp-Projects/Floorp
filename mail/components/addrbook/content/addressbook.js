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
# The Original Code is Mozilla Addressbook.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corp.
# Portions created by the Initial Developer are Copyright (C) 1999-2001
# the Initial Developer. All Rights Reserved.
#
# Original Author:
#   Paul Hangas <hangas@netscape.com>
#
# Contributor(s):
#   Seth Spitzer <sspitzer@netscape.com>
#   Mark Banner <mark@standard8.demon.co.uk>
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

var cvPrefs = 0;
var addressbook = 0;
var gSearchTimer = null;
var gStatusText = null;
var gQueryURIFormat = null;
var gSearchInput;
var gPrintSettings = null;
var gDirTree;
var gCardViewBox;
var gCardViewBoxEmail1;
var gPreviousDirTreeIndex = -1;

// Constants that correspond to choices
// in Address Book->View -->Show Name as
const kDisplayName = 0;
const kLastNameFirst = 1;
const kFirstNameFirst = 2;
const kLDAPDirectory = 0; // defined in nsDirPrefs.h
const kPABDirectory  = 2; // defined in nsDirPrefs.h

// Note: We need to keep this listener as it does not just handle dir
// pane deletes but also deletes of address books and lists from places like
// the sidebar and LDAP preference pane.
var gAddressBookAbListener = {
  onItemAdded: function(parentDir, item) {
    // will not be called
  },
  onItemRemoved: function(parentDir, item) {
    // will only be called when an addressbook is deleted
    try {
      // If we don't have a record of the previous selection, the only
      // option is to select the first.
      if (gPreviousDirTreeIndex == -1) {
        SelectFirstAddressBook();
      }
      else {
        // Don't reselect if we already have a valid selection, this may be
        // the case if items are being removed via other methods, e.g. sidebar,
        // LDAP preference pane etc.
        if (dirTree.currentIndex == -1) {
          var directory = item.QueryInterface(Components.interfaces.nsIAbDirectory);

          // If we are a mail list, move the selection up the list before
          // trying to find the parent. This way we'll end up selecting the
          // parent address book when we remove a mailing list.
          //
          // For simple address books we don't need to move up the list, as
          // we want to select the next one upon removal.
          if (directory.isMailList && gPreviousDirTreeIndex > 0)
            --gPreviousDirTreeIndex;

          // Now get the parent of the row.
          var newRow = dirTree.view.getParentIndex(gPreviousDirTreeIndex);

          // if we have no parent (i.e. we are an address book), use the
          // previous index.
          if (newRow == -1)
            newRow = gPreviousDirTreeIndex;

          // Fall back to the first adddress book if we're not in a valid range
          if (newRow >= dirTree.view.rowCount)
            newRow = 0;

          // Now select the new item.
          dirTree.view.selection.select(newRow);
        }
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
  var prefBranch = prefService.getBranch(null).QueryInterface(Components.interfaces.nsIPrefBranch2);
  prefBranch.addObserver(kPrefMailAddrBookLastNameFirst, gMailAddrBookLastNameFirstObserver, false);
}

function RemovePrefObservers()
{
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefBranch = prefService.getBranch(null).QueryInterface(Components.interfaces.nsIPrefBranch2);
  prefBranch.removeObserver(kPrefMailAddrBookLastNameFirst, gMailAddrBookLastNameFirstObserver);
}

// we won't show the window until the onload() handler is finished
// so we do this trick (suggested by hyatt / blaker)
function OnLoadAddressBook()
{
  setTimeout(delayedOnLoadAddressBook, 0); // when debugging, set this to 5000, so you can see what happens after the window comes up.
}

function delayedOnLoadAddressBook()
{
  gSearchInput = document.getElementById("searchInput");

  verifyAccounts(null); 	// this will do migration, if we need to.

  top.addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);

  InitCommonJS();

  //This migrates the LDAPServer Preferences from 4.x to mozilla format.
  var ldapPrefs = null;
  try {
    ldapPrefs = Components.classes["@mozilla.org/ldapprefs-service;1"]
                          .getService(Components.interfaces.nsILDAPPrefsService);
  } catch (ex) {Components.utils.reportError("ERROR: Cannot get the LDAP service\n" + ex + "\n");}

  if (ldapPrefs) {
    ldapPrefs.migratePrefsIfNeeded();
  }

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
  // this listener cares when a directory (= address book), or a directory item
  // is/are removed. In the case of directory items, we are only really
  // interested in mailing list changes and not cards but we have to have both.
  addrbookSession.addAddressBookListener(
    gAddressBookAbListener,
    Components.interfaces.nsIAddrBookSession.directoryRemoved |
    Components.interfaces.nsIAddrBookSession.directoryItemRemoved);

  var dirTree = GetDirTree();
  dirTree.addEventListener("click",DirPaneClick,true);

  // initialize the customizeDone method on the customizeable toolbar
  var toolbox = document.getElementById("ab-toolbox");
  toolbox.customizeDone = MailToolboxCustomizeDone;

  var toolbarset = document.getElementById('customToolbars');
  toolbox.toolbarset = toolbarset;

  // Ensure we don't load xul error pages into the main window
  window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
        .getInterface(Components.interfaces.nsIWebNavigation)
        .QueryInterface(Components.interfaces.nsIDocShell)
        .useErrorPages = false;
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

function onFileMenuInit()
{
  goUpdateCommand('cmd_printcard'); 
  goUpdateCommand('cmd_printcardpreview');
}

function CommandUpdate_AddressBook()
{
  goUpdateCommand('cmd_delete');
  goUpdateCommand('button_delete');
  goUpdateCommand('cmd_newlist');
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
    gPrintSettings = PrintUtils.getPrintSettings();
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
  var url = "data:application/xml;base64," + card.convertToBase64EncodedXML();
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
    gPrintSettings = PrintUtils.getPrintSettings();
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
 
  var parentRow = GetParentRow(dirTree, dirTree.currentIndex);
  var parentId = (parentRow == -1) ? "moz-abdirectory://" : dirTree.builderView.getResourceAtIndex(parentRow).Value;
  var parentDir = GetDirectoryFromURI(parentId);
  parentArray.AppendElement(parentDir);
    
  var directory = GetDirectoryFromURI(selectedABURI);
  var confirmDeleteMessage;
  var clearPrefsRequired = false;

  if (directory.isMailList)
    confirmDeleteMessage = gAddressBookBundle.getString("confirmDeleteMailingList");
  else {
    // Check if this address book is being used for collection
    if (gPrefs.getCharPref("mail.collect_addressbook") == selectedABURI &&
        (gPrefs.getBoolPref("mail.collect_email_address_outgoing") ||
         gPrefs.getBoolPref("mail.collect_email_address_newsgroup"))) {
      var brandShortName = document.getElementById("bundle_brand").getString("brandShortName");

      confirmDeleteMessage = gAddressBookBundle.getFormattedString("confirmDeleteCollectionAddressbook", [brandShortName]);
      clearPrefsRequired = true;
    }
    else {
      confirmDeleteMessage = gAddressBookBundle.getString("confirmDeleteAddressbook");
    }
  }

  if (!promptService.confirm(window,
                             gAddressBookBundle.getString(
                                                directory.isMailList ?
                                                "confirmDeleteMailingListTitle" :
                                                "confirmDeleteAddressbookTitle"),
                             confirmDeleteMessage))
    return;

  // First clear all the prefs if required
  if (clearPrefsRequired) {
    gPrefs.setBoolPref("mail.collect_email_address_outgoing", false);
    gPrefs.setBoolPref("mail.collect_email_address_newsgroup", false);

    // Also reset the displayed value so that we don't get a blank item in the
    // prefs dialog if it gets enabled.
    gPrefs.setCharPref("mail.collect_addressbook", kPersonalAddressbookURI);
  }

  var resourceArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  var selectedABResource = GetDirectoryFromURI(selectedABURI).QueryInterface(Components.interfaces.nsIRDFResource);
  resourceArray.AppendElement(selectedABResource);

  top.addressbook.deleteAddressBooks(dirTree.database, parentArray, resourceArray);
}

function SetStatusText(total)
{
  if (!gStatusText)
    gStatusText = document.getElementById('statusText');

  try {
    var statusText;

    if (gSearchInput && gSearchInput.value) {
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

function AbResultsPaneKeyPress(event)
{
  if (event.keyCode == 13)
    AbEditSelectedCard();
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
  var searchBox         = document.getElementById('search-container');
  var dirTree           = GetDirTree();
  var searchInput       = GetSearchInput();

  if (event && event.shiftKey)
  {
    if (focusedElement == gAbResultsTree && searchBox)
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
    else if (searchBox)
      searchInput.focus();
    else
      gAbResultsTree.focus();
  }
}

function WhichPaneHasFocus()
{
  var cardViewBox       = GetCardViewBox();
  var searchBox         = document.getElementById('search-container');
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
  // Doesn't matter if this bit fails, window.location contains its own prompts
  try {
    window.location = url;
  }
  catch (ex) {}
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

