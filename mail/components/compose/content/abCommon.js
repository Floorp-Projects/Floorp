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

var gResultsTree = 0;
var dirTree = 0;
var abList = 0;
var gAbView = null;
var gSearchInput;
var gSearchTimer = null;
var gQueryURIFormat = null;

var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
var gPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
var gHeaderParser = Components.classes["@mozilla.org/messenger/headerparser;1"].getService(Components.interfaces.nsIMsgHeaderParser);

const kDefaultSortColumn = "GeneratedName";
const kDefaultAscending = "ascending";
const kDefaultDescending = "descending";
const kPersonalAddressbookURI = "moz-abmdbdirectory://abook.mab";
const kCollectedAddressbookURI = "moz-abmdbdirectory://history.mab";

// Controller object for Results Pane
var ResultsPaneController =
{
  supportsCommand: function(command)
  {
    switch (command) {
      case "cmd_selectAll":
      case "cmd_delete":
      case "button_delete":
      case "button_edit":
        return true;
      default:
        return false;
    }
  },

  isCommandEnabled: function(command)
  {
    switch (command) {
      case "cmd_selectAll":
        return true;

      case "cmd_delete":
      case "button_delete":
        var numSelected;
        var enabled = false;
        if (gAbView && gAbView.selection) {
          if (gAbView.directory)         
            enabled = gAbView.directory.operations & gAbView.directory.opWrite;
          numSelected = gAbView.selection.count;
        }
        else 
          numSelected = 0;

        // fix me, don't update on isCommandEnabled
        if (command == "cmd_delete") {
          if (numSelected < 2)
            goSetMenuValue(command, "valueCard");
          else
            goSetMenuValue(command, "valueCards");
        }
        return (enabled && (numSelected > 0));
      case "button_edit":
        return (GetSelectedCardIndex() != -1);
      default:
        return false;
    }
  },

  doCommand: function(command)
  {
    switch (command) {
      case "cmd_selectAll":
        if (gAbView)
          gAbView.selectAll();
        break;
      case "cmd_delete":
      case "button_delete":
        AbDelete();
        break;
      case "button_edit":
        AbEditSelectedCard();
        break;
    }
  },

  onEvent: function(event)
  {
    // on blur events set the menu item texts back to the normal values
    if (event == "blur")
      goSetMenuValue("cmd_delete", "valueDefault");
  }
};


// Controller object for Dir Pane
var DirPaneController =
{
  supportsCommand: function(command)
  {
    switch (command) {
      case "cmd_selectAll":
      case "cmd_delete":
      case "button_delete":
      case "button_edit":
        return true;
      default:
        return false;
    }
  },

  isCommandEnabled: function(command)
  {
    switch (command) {
      case "cmd_selectAll":
        return true;
      case "cmd_delete":
      case "button_delete":
        if (command == "cmd_delete")
          goSetMenuValue(command, "valueAddressBook");
        if (dirTree && dirTree.currentIndex >= 0)
          return true;
        else
          return false;
      case "button_edit":
        var selectedDir = GetSelectedDirectory();
        if (selectedDir) {
          var directory = GetDirectoryFromURI(selectedDir);
          if ((directory.isMailList) || 
              (!(directory.operations & directory.opWrite)))
             return true;
        }
        return false;
      default:
        return false;
    }
  },

  doCommand: function(command)
  {
    switch (command) {
      case "cmd_selectAll":
        if (dirTree)
          dirTree.view.selection.selectAll();
        break;

      case "cmd_delete":
      case "button_delete":
        if (dirTree)
          AbDeleteDirectory();
        break;
      case "button_edit":
        AbEditSelectedDirectory();
        break;
    }
  },

  onEvent: function(event)
  {
    // on blur events set the menu item texts back to the normal values
    if (event == "blur")
      goSetMenuValue("cmd_delete", "valueDefault");
  }
};

function AbEditSelectedDirectory()
{
  if (dirTree.view.selection.count == 1) {
    var mailingListUri = GetSelectedDirectory();
    var directory = GetDirectoryFromURI(mailingListUri);
    if (directory.isMailList) {
      var dirUri = GetParentDirectoryFromMailingListURI(mailingListUri);
      goEditListDialog(dirUri, null, mailingListUri, UpdateCardView);
    }
    else if (!(directory.operations & directory.opWrite))
    {
      var ldapUrlPrefix = "moz-abldapdirectory://";
      if ((mailingListUri.indexOf(ldapUrlPrefix, 0)) == 0)
      {
        var args = { selectedDirectory: directory.dirName,
                     selectedDirectoryString: null};
        args.selectedDirectoryString = mailingListUri.substr(ldapUrlPrefix.length, mailingListUri.length);
        window.openDialog("chrome://messenger/content/addressbook/pref-directory-add.xul",
                      "editDirectory", "chrome,modal=yes,resizable=no,centerscreen", args);
      }
    }
  }
}

function GetParentRow(aTree, aRow)
{
  var row = aRow;
  var level = aTree.view.getLevel(row);
  var parentLevel = level;
  while (parentLevel >= level) {
    row--;
    if (row == -1)
      return row;
    parentLevel = aTree.view.getLevel(row);
  }
  return row;
}
        
function InitCommonJS()
{
  dirTree = document.getElementById("dirTree");
  //abList = document.getElementById("addressbookList");
  gResultsTree = document.getElementById("abResultsTree");
}

// builds prior to 12-08-2001 did not use an tree for
// the results pane.  so for any existing profiles will 
// get all columns, whereas new profile only get a select few
// because we hide them by default in localStore.rdf
// to work around this, we hide the non-default columns once.
// there is more than one results pane (addressbook, select addresses,
// addressbook sidebar channel, etc) so we'll pass in the 
// the pref so that we'll migrate each of them once.
function UpgradeAddressBookResultsPaneUI(prefName)
{
  var resultsPaneUIVersion;

  try {
    resultsPaneUIVersion = gPrefs.getIntPref(prefName);
    if (resultsPaneUIVersion == 1) {
      // hide all columns with hiddenbydefault="true" 
      var elements = document.getElementsByAttribute("hiddenbydefault","true");
      for (var i=0; i<elements.length; i++) {
        elements[i].setAttribute("hidden","true");
      }
      gPrefs.setIntPref(prefName, 2);
    }
  }
  catch (ex) {
    dump("UpgradeAddressBookResultsPaneUI " + prefName + " ex = " + ex + "\n");
  }
}

function SetupAbCommandUpdateHandlers()
{
  // dir pane
  if (dirTree)
    dirTree.controllers.appendController(DirPaneController);

  // results pane
  if (gResultsTree)
    gResultsTree.controllers.appendController(ResultsPaneController);
}

function AbDelete()
{
  if (gAbView) 
    gAbView.deleteSelectedCards();
}

function AbNewCard(abListItem)
{
  var selectedAB = GetSelectedAddressBookDirID(abListItem);
  goNewCardDialog(selectedAB);
}

// NOTE, will return -1 if more than one card selected, or no cards selected.
function GetSelectedCardIndex()
{
  if (!gAbView)
    return -1;

  var treeSelection = gAbView.selection;
  if (treeSelection.getRangeCount() == 1) {
    var start = new Object;
    var end = new Object;
    treeSelection.getRangeAt(0,start,end);
    if (start.value == end.value)
      return start.value;
  }

  return -1;
}

// NOTE, returns the card if exactly one card is selected, null otherwise
function GetSelectedCard()
{
  var index = GetSelectedCardIndex();
  if (index == -1)
    return null;
  else 
    return gAbView.getCardFromRow(index);
}

function AbEditSelectedCard()
{
dump("AbEditSelectedCard\n");
  AbEditCard(GetSelectedCard());
}

function AbEditCard(card)
{
  if (!card)
    return;

  if (card.isMailList) {
    goEditListDialog(gAbView.URI, card, card.mailListURI, UpdateCardView);
  }
  else {
    goEditCardDialog(gAbView.URI, card, UpdateCardView);
  }
}

function AbNewMessage()
{
  var msgComposeType = Components.interfaces.nsIMsgCompType;
  var msgComposFormat = Components.interfaces.nsIMsgCompFormat;
  var msgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService();
  msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

  var params = Components.classes["@mozilla.org/messengercompose/composeparams;1"].createInstance(Components.interfaces.nsIMsgComposeParams);
  if (params)
  {
    params.type = msgComposeType.New;
    params.format = msgComposFormat.Default;
    var composeFields = Components.classes["@mozilla.org/messengercompose/composefields;1"].createInstance(Components.interfaces.nsIMsgCompFields);
    if (composeFields)
    {
      composeFields.to = GetSelectedAddresses();
      params.composeFields = composeFields;
      msgComposeService.OpenComposeWindowWithParams(null, params);
    }
  }
}

function GetOneOrMoreCardsSelected()
{
  if (!gAbView)
    return false;

  return (gAbView.selection.getRangeCount() > 0);
}

// XXX todo
// could this be moved into utilityOverlay.js?
function goToggleSplitter( id, elementID )
{
  var splitter = document.getElementById( id );
  var element = document.getElementById( elementID );
  if ( splitter )
  {
    var attribValue = splitter.getAttribute("state") ;

    if ( attribValue == "collapsed" )
    {
      splitter.setAttribute("state", "open" );
      if ( element )
        element.setAttribute("checked","true")
    }
    else
    {
      splitter.setAttribute("state", "collapsed");
      if ( element )
        element.setAttribute("checked","false")
    }
    document.persist(id, 'state');
    document.persist(elementID, 'checked');
  }
}

// Generate a list of cards from the selected mailing list 
// and get a comma separated list of card addresses. If the
// item selected in the directory pane is not a mailing list,
// an empty string is returned. 
function GetSelectedAddressesFromDirTree() 
{
  var addresses = "";

  if (dirTree.currentIndex >= 0) {
    var selected = dirTree.contentView.getItemAtIndex(dirTree.currentIndex);
    var mailingListUri = selected.id;
    var directory = GetDirectoryFromURI(mailingListUri);
    if (directory.isMailList) {
      var listCardsCount = directory.addressLists.Count();
      var cards = new Array(listCardsCount);
      for ( var i = 0;  i < listCardsCount; i++ ) {
        var card = directory.addressLists.GetElementAt(i);
        card = card.QueryInterface(Components.interfaces.nsIAbCard);
        cards[i] = card;
      }
      addresses = GetAddressesForCards(cards);
    }
  }
  return addresses;
}

function GetSelectedAddresses()
{
  var selectedCards = GetSelectedAbCards();
  return GetAddressesForCards(selectedCards);
}

// Generate a comma seperate list of addresses from a given
// set of cards.
function GetAddressesForCards(cards)
{
  var addresses = "";

  if (!cards)
    return addresses;

  var count = cards.length;
  if (count > 0)
    addresses += GenerateAddressFromCard(cards[0]);

  for (var i = 1; i < count; i++) { 
    var generatedAddress = GenerateAddressFromCard(cards[i]);

    if (generatedAddress)
      addresses += ", " + generatedAddress;
  }
  return addresses;
}

function GetNumSelectedCards()
{
 try {
   var treeSelection = gAbView.selection;
   return treeSelection.count;
 }
 catch (ex) {
 }

 // if something went wrong, return 0 for the count.
 return 0;
}

// XXX todo
// an optimization might be to make this return 
// the selected ranges, which would be faster
// when the user does large selections, but for now, let's keep it simple.
function GetSelectedRows()
{
  var selectedRows = "";

  if (!gAbView)
    return selectedRows;

  var i,j;
  var rangeCount = gAbView.selection.getRangeCount();
  var current = 0;

  for (i=0; i < rangeCount; i++) {
    var start = new Object;
    var end = new Object;
    gAbView.selection.getRangeAt(i,start,end);
    for (j=start.value;j<=end.value;j++) {
      if (selectedRows)
        selectedRows += ",";
      selectedRows += j;
    }
  }
  return selectedRows;
}

function GetSelectedAbCards()
{
  var abView = gAbView;

  // if sidebar is open, and addressbook panel is open and focused,
  // then use the ab view from sidebar (gCurFrame is from sidebarOverlay.js)
  const abPanelUrl = "chrome://messenger/content/addressbook/addressbook-panel.xul";
  if (document.getElementById("sidebar-box")) {
    if (gCurFrame && 
        gCurFrame.getAttribute("src") == abPanelUrl &&
        document.commandDispatcher.focusedWindow == gCurFrame.contentDocument.defaultView) 
    {
      abView = gCurFrame.contentDocument.defaultView.gAbView;
    }
  }

  if (!abView)
    return null;

  var cards = new Array(abView.selection.count);
  var i,j;
  var count = abView.selection.getRangeCount();

  var current = 0;

  for (i=0; i < count; i++) {
    var start = new Object;
    var end = new Object;
    abView.selection.getRangeAt(i,start,end);
    for (j=start.value;j<=end.value;j++) {
      cards[current] = abView.getCardFromRow(j);
      current++;
    }
  }
  return cards;
}

function SelectFirstAddressBook()
{
  ChangeDirectoryByURI(kPersonalAddressbookURI);
}

function SelectFirstCard()
{
  if (gAbView && gAbView.selection) {
    gAbView.selection.select(0);
  }
}

function DirPaneClick(event)
{
  // we only care about left button events
  if (event.button != 0)
    return;

  var searchInput = document.getElementById("searchInput");
  // if there is a searchInput element, and it's not blank 
  // then we need to act like the user cleared the
  // search text
  if (searchInput && searchInput.value) {
    searchInput.value = "";
    onEnterInSearchBar();
  }
}

function DirPaneDoubleClick()
{
  if (dirTree && dirTree.view.selection && dirTree.view.selection.count == 1)
    AbEditSelectedDirectory();
}

function DirPaneSelectionChange()
{
  if (dirTree && dirTree.view.selection && dirTree.view.selection.count == 1)
    ChangeDirectoryByURI(GetSelectedDirectory());
}

function GetAbResultsBoxObject()
{
  return GetAbResultsTree().treeBoxObject;
}

var gAbResultsTree = null;

function GetAbResultsTree()
{
  if (gAbResultsTree) 
    return gAbResultsTree;

  gAbResultsTree = document.getElementById('abResultsTree');
  return gAbResultsTree;
}

function CloseAbView()
{
  var boxObject = GetAbResultsBoxObject();
  boxObject.view = null;

  if (gAbView) {
    gAbView.close();
    gAbView = null;
  }
}

function SetAbView(uri, sortColumn, sortDirection)
{
  CloseAbView();

  if (!sortColumn)
    sortColumn = kDefaultSortColumn;

  if (!sortDirection)
    sortDirection = kDefaultAscending;

  gAbView = Components.classes["@mozilla.org/addressbook/abview;1"].createInstance(Components.interfaces.nsIAbView);
 
  var actualSortColumn = gAbView.init(uri, GetAbViewListener(), sortColumn, sortDirection);
  var boxObject = GetAbResultsBoxObject();
  boxObject.view = gAbView.QueryInterface(Components.interfaces.nsITreeView);
  return actualSortColumn;
}

function GetAbView()
{
  return gAbView;
}

function GetAbViewURI()
{
  if (gAbView)
    return gAbView.URI;
  else 
    return null;
}

function ChangeDirectoryByURI(uri)
{
  if (!uri)
    uri = kPersonalAddressbookURI;

  if (gAbView && gAbView.URI == uri)
    return;
  
  var dataNode = document.getElementById(uri);
  var sortColumn = dataNode ? dataNode.getAttribute("sortColumn") : kDefaultSortColumn;
  var sortDirection = dataNode ? dataNode.getAttribute("sortDirection") : kDefaultAscending;
  
  var actualSortColumn = SetAbView(uri, sortColumn, sortDirection);

  UpdateSortIndicators(actualSortColumn, sortDirection);
  
  // only select the first card if there is a first card
  if (gAbView && gAbView.getCardFromRow(0)) {
    SelectFirstCard();
  }
  else {
    // the selection changes if we were switching directories.
    ResultsPaneSelectionChanged()
  }
  return;
}

function AbSortAscending()
{
  var sortColumn = kDefaultSortColumn;

  if (gAbView) {
    var node = document.getElementById(gAbView.URI);
    sortColumn = node.getAttribute("sortColumn");
  }

  SortAndUpdateIndicators(sortColumn, kDefaultAscending);
}

function AbSortDescending()
{
  var sortColumn = kDefaultSortColumn;

  if (gAbView) {
    var node = document.getElementById(gAbView.URI);
    sortColumn = node.getAttribute("sortColumn");
  }

  SortAndUpdateIndicators(sortColumn, kDefaultDescending);
}

function SortResultPane(sortColumn)
{
  var sortDirection = kDefaultAscending;

  if (gAbView) {
     sortDirection = gAbView.sortDirection;
  }

  SortAndUpdateIndicators(sortColumn, sortDirection);
}

function SortAndUpdateIndicators(sortColumn, sortDirection)
{
  // XXX todo remove once #116341 is fixed
  if (!sortColumn)
    return;
    
  UpdateSortIndicators(sortColumn, sortDirection);

  if (gAbView)
    gAbView.sortBy(sortColumn, sortDirection);

  SaveSortSetting(sortColumn, sortDirection);
}

function SaveSortSetting(column, direction)
{
  if (dirTree && gAbView) {
    var node = document.getElementById(gAbView.URI);
    if (node) {
      node.setAttribute("sortColumn", column);
      node.setAttribute("sortDirection", direction);
    }
  }
}

function UpdateSortIndicators(colID, sortDirection)
{
  var sortedColumn;
  // set the sort indicator on the column we are sorted by
  if (colID) {
    sortedColumn = document.getElementById(colID);
    if (sortedColumn) {
      sortedColumn.setAttribute("sortDirection",sortDirection);
    }
  }

  // remove the sort indicator from all the columns
  // except the one we are sorted by
  var currCol = GetAbResultsTree().firstChild.firstChild;
  while (currCol) {
    if (currCol != sortedColumn && currCol.localName == "treecol")
      currCol.removeAttribute("sortDirection");
    currCol = currCol.nextSibling;
  }
}

function GetAbResultsTree()
{
  if (!gAbResultsTree)
    gAbResultsTree = document.getElementById('abResultsTree');
  return gAbResultsTree;
}


function InvalidateResultsPane()
{
  var tree = GetAbResultsTree();
  if (tree) {
    tree.treeBoxObject.invalidate();
  }
}

function AbNewList(abListItem)
{
  var selectedAB = GetSelectedAddressBookDirID(abListItem);

  goNewListDialog(selectedAB);
}

function GetSelectedAddressBookDirID(abListItem)
{
  /*var selectedAB = 0;
  var abDirEntries = document.getElementById(abListItem);

  if (abDirEntries && abDirEntries.localName == "tree" && abDirEntries.currentIndex >= 0) {
    var selected = abDirEntries.contentView.getItemAtIndex(abDirEntries.currentIndex);
    selectedAB = selected.id;
  }

  if (!selectedAB && abDirEntries && abDirEntries.localName == "menulist" && abDirEntries.selectedItem)
    selectedAB = abDirEntries.selectedItem.getAttribute("id");

  return selectedAB;*/

  //var addressbookList = document.getElementById("addressbookList");

  //return addressbookList.selectedItem.id;
  
  return kPersonalAddressbookURI;
}

function goNewListDialog(selectedAB)
{
  window.openDialog("chrome://messenger/content/addressbook/abMailListDialog.xul",
                    "",
                    "chrome,resizable=no,titlebar,modal,centerscreen",
                    {selectedAB:selectedAB});
}

function goEditListDialog(abURI, abCard, listURI, okCallback)
{
  window.openDialog("chrome://messenger/content/addressbook/abEditListDialog.xul",
                    "",
                    "chrome,resizable=no,titlebar,modal,centerscreen",
                    {abURI:abURI, abCard:abCard, listURI:listURI, okCallback:okCallback});
}

function goNewCardDialog(selectedAB)
{
  window.openDialog("chrome://messenger/content/addressbook/abNewCardDialog.xul",
                    "",
                    "chrome,resizable=no,titlebar,modal,centerscreen",
                    {selectedAB:selectedAB});
}

function goEditCardDialog(abURI, card, okCallback)
{
  window.openDialog("chrome://messenger/content/addressbook/abEditCardDialog.xul",
					  "",
					  "chrome,resizable=no,modal,titlebar,centerscreen",
					  {abURI:abURI, card:card, okCallback:okCallback});
}


function setSortByMenuItemCheckState(id, value)
{
    var menuitem = document.getElementById(id);
    if (menuitem) {
      menuitem.setAttribute("checked", value);
    }
}

function InitViewSortByMenu()
{
    var sortColumn = kDefaultSortColumn;
    var sortDirection = kDefaultAscending;

    if (gAbView) {
      sortColumn = gAbView.sortColumn;
      sortDirection = gAbView.sortDirection;
    }

    // this approach is necessary to support generic columns that get overlayed.
    var elements = document.getElementsByAttribute("name","sortas");
    for (var i=0; i<elements.length; i++) {
        var cmd = elements[i].getAttribute("id");
        var columnForCmd = cmd.split("cmd_SortBy")[1];
        setSortByMenuItemCheckState(cmd, (sortColumn == columnForCmd));
    }

    setSortByMenuItemCheckState("sortAscending", (sortDirection == kDefaultAscending));
    setSortByMenuItemCheckState("sortDescending", (sortDirection == kDefaultDescending));
}

function GenerateAddressFromCard(card)
{
  if (!card)
    return "";

  var email;

  if (card.isMailList) 
  {
    var directory = GetDirectoryFromURI(card.mailListURI);
    if(directory.description)
      email = directory.description;
    else
      email = card.displayName;
  }
  else {
    email = card.primaryEmail;
  }

  return gHeaderParser.makeFullAddressWString(card.displayName, email);
}

function GetDirectoryFromURI(uri)
{
  var directory = rdf.GetResource(uri).QueryInterface(Components.interfaces.nsIAbDirectory);
  return directory;
}

// returns null if abURI is not a mailing list URI
function GetParentDirectoryFromMailingListURI(abURI)
{
  var abURIArr = abURI.split("/");
  /*
   turn turn "moz-abmdbdirectory://abook.mab/MailList6"
   into ["moz-abmdbdirectory:","","abook.mab","MailList6"]
   then, turn ["moz-abmdbdirectory:","","abook.mab","MailList6"]
   into "moz-abmdbdirectory://abook.mab"
  */
  if (abURIArr.length == 4 && abURIArr[0] == "moz-abmdbdirectory:" && abURIArr[3] != "") {
    return abURIArr[0] + "/" + abURIArr[1] + "/" + abURIArr[2];
  }

  return null;
} 

function DirPaneHasFocus()
{
  // returns true if diectory pane has the focus. Returns false, otherwise.
  return (top.document.commandDispatcher.focusedElement == dirTree)
}

function GetSelectedDirectory()
{
  //var addressbookList = document.getElementById("addressbookList");

  //return addressbookList.selectedItem.id;

  return kPersonalAddressbookURI;
}

function onAbSearchKeyPress(event)
{
  // 13 == return
  if (event && event.keyCode == 13) 
    onAbSearchInput(true);
}
    
function onAbSearchInput(returnKeyHit)
{
  SearchInputChanged();

  if (gSearchTimer) {
    clearTimeout(gSearchTimer);
    gSearchTimer = null;
  }

  if (returnKeyHit) {
    onEnterInSearchBar();
  }
  else {
    gSearchTimer = setTimeout("onEnterInSearchBar();", 800);
  }
}

function SearchInputChanged() 
{
  if (!gSearchInput)
    gSearchInput = document.getElementById("searchInput")

  var clearButton = document.getElementById("clear");
  if (clearButton) {
    if (gSearchInput.value && (gSearchInput.value != ""))
      clearButton.removeAttribute("disabled");
    else
      clearButton.setAttribute("disabled", "true");
  }
}

function onAbClearSearch() 
{
  if (!gSearchInput)
    gSearchInput = document.getElementById("searchInput")

  if (gSearchInput) 
    gSearchInput.value ="";  //on input does not get fired for some reason
  
  onAbSearchInput(true);
  gSearchInput.focus();
}

function ChangeAddressbookListing()
{
  if (!gSearchInput)
    gSearchInput = document.getElementById("searchInput")

  //var addressbookList = document.getElementById("addressbookList");
  //if (addressbookList && addressbookList.selectedItem) {
    if (gSearchInput.value && (gSearchInput.value != ""))
      onEnterInSearchBar();
    else 
      //ChangeDirectoryByURI(addressbookList.selectedItem.id);
      ChangeDirectoryByURI(kPersonalAddressbookURI);
  //}
}

function onEnterInSearchBar()
{
  if ("ClearCardViewPane" in window)
    ClearCardViewPane();

  if (!gQueryURIFormat)
    gQueryURIFormat = gPrefs.getCharPref("mail.addr_book.quicksearchquery.format");

  if (!gSearchInput)
    gSearchInput = document.getElementById("searchInput")

  var searchURI = GetSelectedDirectory();
  if (!searchURI) return;

  var addressbookList = document.getElementById("addressbookList");

  var selectedNode = addressbookList.selectedItem;
  var sortColumn = selectedNode.getAttribute("sortColumn");
  var sortDirection = selectedNode.getAttribute("sortDirection");

  if (gSearchInput.value != "") {
    searchURI += gQueryURIFormat.replace(/@V/g, encodeURIComponent(gSearchInput.value));
  }

  SetAbView(searchURI, sortColumn, sortDirection);
  
  SelectFirstCard();
}

