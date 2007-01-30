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

var dirTree = 0;
var abList = 0;
var gAbResultsTree = null;
var gAbView = null;
var gAddressBookBundle;
var gCurDirectory;

var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
var gPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
var gHeaderParser = Components.classes["@mozilla.org/messenger/headerparser;1"].getService(Components.interfaces.nsIMsgHeaderParser);

const kDefaultSortColumn = "GeneratedName";
const kDefaultAscending = "ascending";
const kDefaultDescending = "descending";
const kPersonalAddressbookURI = "moz-abmdbdirectory://abook.mab";
const kCollectedAddressbookURI = "moz-abmdbdirectory://history.mab";

// List/card selections in the results pane.
const kNothingSelected = 0;
const kListsAndCards = 1;
const kMultipleListsOnly = 2;
const kSingleListOnly = 3;
const kCardsOnly = 4;

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
      case "cmd_printcard":
      case "cmd_printcardpreview":
      case "cmd_newlist":
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
      case "cmd_printcard":
      case "cmd_printcardpreview":
      case "button_edit":
        return (GetSelectedCardIndex() != -1);
      case "cmd_newlist":
        var selectedDir = GetSelectedDirectory();
        if (selectedDir) {
          var abDir = GetDirectoryFromURI(selectedDir);
          if (abDir) {
            return abDir.supportsMailingLists;
          }
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
      case "cmd_printcard":
        AbPrintCard();
        break;
      case "cmd_printcardpreview":
        AbPrintPreviewCard();
        break;
      case "cmd_newlist":
        AbNewList();
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
      case "cmd_printcard":
      case "cmd_printcardpreview":
      case "cmd_newlist":
        return true;
      default:
        return false;
    }
  },

  isCommandEnabled: function(command)
  {
    var selectedDir;

    switch (command) {
      case "cmd_selectAll":
        // the dirTree pane
        // only handles single selection
        // so we forward select all to the results pane
        // but if there is no gAbView
        // don't bother sending to the results pane
        return (gAbView != null);
      case "cmd_delete":
      case "button_delete":
        if (command == "cmd_delete")
          goSetMenuValue(command, "valueAddressBook");
        
        selectedDir = GetSelectedDirectory();
        
        if (selectedDir == kPersonalAddressbookURI || selectedDir == kCollectedAddressbookURI)
          return false;

        if (selectedDir) {
          // If the selected directory is an ldap directory
          // and if the prefs for this directory are locked
          // disable the delete button.
          var ldapUrlPrefix = "moz-abldapdirectory://";
          if ((selectedDir.indexOf(ldapUrlPrefix, 0)) == 0)
          {
            var disable = false;
            try {
              var prefName = selectedDir.substr(ldapUrlPrefix.length);
              disable = gPrefs.getBoolPref(prefName + ".disable_delete");
            }
            catch(ex) {
              // if this preference is not set its ok.
            }
            if (disable)
              return false;
          }
          return true;
        }
        else
          return false;
      case "cmd_printcard":
      case "cmd_printcardpreview":
        return (GetSelectedCardIndex() != -1);
      case "button_edit":
        return (GetSelectedDirectory() != null);
      case "cmd_newlist":
        selectedDir = GetSelectedDirectory();
        if (selectedDir) {
          var abDir = GetDirectoryFromURI(selectedDir);
          if (abDir) {
            return abDir.supportsMailingLists;
          }
        }
        return false;
      default:
        return false;
    }
  },

  doCommand: function(command)
  {
    switch (command) {
      case "cmd_printcard":
      case "cmd_printcardpreview":
      case "cmd_selectAll":
        SendCommandToResultsPane(command);
        break;
      case "cmd_delete":
      case "button_delete":
        if (dirTree)
          AbDeleteDirectory();
        break;
      case "button_edit":
        AbEditSelectedDirectory();
        break;
      case "cmd_newlist":
        AbNewList();
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

function SendCommandToResultsPane(command)
{
  ResultsPaneController.doCommand(command);

  // if we are sending the command so the results pane
  // we should focus the results pane
  gAbResultsTree.focus();
}

function AbEditSelectedDirectory()
{
  if (dirTree.view.selection.count == 1) {
    var selecteduri = GetSelectedDirectory();
    var directory = GetDirectoryFromURI(selecteduri);
    if (directory.isMailList) {
      var dirUri = GetParentDirectoryFromMailingListURI(selecteduri);
      goEditListDialog(dirUri, null, selecteduri, UpdateCardView);
    }
    else {
        if (directory instanceof Components.interfaces.nsIAbLDAPDirectory) {
        var ldapUrlPrefix = "moz-abldapdirectory://";
        var args = { selectedDirectory: directory.dirName,
                     selectedDirectoryString: null};
        args.selectedDirectoryString = selecteduri.substr(ldapUrlPrefix.length);
        window.openDialog("chrome://messenger/content/addressbook/pref-directory-add.xul",
                      "editDirectory", "chrome,modal=yes,resizable=no,centerscreen", args);
      }
      else {
        AbRenameAddressBook();
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
  abList = document.getElementById("addressbookList");
  gAbResultsTree = document.getElementById("abResultsTree");
  gAddressBookBundle = document.getElementById("bundle_addressBook");
}

function SetupAbCommandUpdateHandlers()
{
  // dir pane
  if (dirTree)
    dirTree.controllers.appendController(DirPaneController);

  // results pane
  if (gAbResultsTree)
    gAbResultsTree.controllers.appendController(ResultsPaneController);
}

function GetSelectedCardTypes()
{
  var cards = GetSelectedAbCards();
  if (!cards)
    return kNothingSelected; // no view

  var count = cards.length;
  if (count == 0)
    return kNothingSelected;  // nothing selected

  var mailingListCnt = 0;
  var cardCnt = 0;
  for (var i = 0; i < count; i++) { 
    if (cards[i].isMailList)
      mailingListCnt++;
    else
      cardCnt++;
  }
  if (mailingListCnt && cardCnt)
    return kListsAndCards;        // lists and cards selected
  else if (mailingListCnt && !cardCnt) {
    if (mailingListCnt > 1)
      return kMultipleListsOnly; // only multiple mailing lists selected
    else
      return kSingleListOnly;    // only single mailing list
  }
  else if (!mailingListCnt && cardCnt)
    return kCardsOnly;           // only card(s) selected

  // Fallback just in case.
  return kNothingSelected;
}

function AbDelete()
{
  var types = GetSelectedCardTypes();
  if (types == kNothingSelected)
    return;

  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  // If at least one mailing list is selected then prompt users for deletion.
  if (types != kCardsOnly)
  {
    var confirmDeleteMessage;
    if (types == kListsAndCards)
      confirmDeleteMessage = gAddressBookBundle.getString("confirmDeleteListsAndCards");
    else if (types == kMultipleListsOnly)
      confirmDeleteMessage = gAddressBookBundle.getString("confirmDeleteMailingLists");
    else
      confirmDeleteMessage = gAddressBookBundle.getString("confirmDeleteMailingList");
    if (!promptService.confirm(window, null, confirmDeleteMessage))
      return;
  }

  gAbView.deleteSelectedCards();
}

function AbNewCard()
{
  goNewCardDialog(GetSelectedDirectory());
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
  return (index == -1) ? null : gAbView.getCardFromRow(index);
}

function AbEditSelectedCard()
{
  AbEditCard(GetSelectedCard());
}

function AbEditCard(card)
{
  // Need a card,
  // but not allowing AOL special groups to be edited.
  if (!card)
    return;

  if (card.isMailList) {
    goEditListDialog(GetSelectedDirectory(), card, card.mailListURI, UpdateCardView);
  }
  else {
    goEditCardDialog(GetSelectedDirectory(), card, UpdateCardView);
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
      if (DirPaneHasFocus())
        composeFields.to = GetSelectedAddressesFromDirTree();
      else
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
    var selectedResource = dirTree.builderView.getResourceAtIndex(dirTree.currentIndex);
    var directory = GetDirectoryFromURI(selectedResource.Value);
    if (directory.isMailList) {
      var listCardsCount = directory.addressLists.Count();
      var cards = new Array(listCardsCount);
      for (var i = 0; i < listCardsCount; ++i)
        cards[i] = directory.addressLists.QueryElementAt(
                     i, Components.interfaces.nsIAbCard);
      addresses = GetAddressesForCards(cards);
    }
  }
  return addresses;
}

function GetSelectedAddresses()
{
  return GetAddressesForCards(GetSelectedAbCards());
}

// Generate a comma separated list of addresses from a given
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
      addresses += "," + generatedAddress;
  }
  return addresses;
}

function GetNumSelectedCards()
{
 try {
   return gAbView.selection.count;
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
    for (j = start.value; j <= end.value; ++j)
      cards[current++] = abView.getCardFromRow(j);
  }
  return cards;
}

function SelectFirstAddressBook()
{
  dirTree.view.selection.select(0);

  ChangeDirectoryByURI(GetSelectedDirectory());
  gAbResultsTree.focus();
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

  // if the user clicks on the header / trecol, do nothing
  if (event.originalTarget.localName == "treecol") {
    event.stopPropagation();
    return;
  }
}

function DirPaneDoubleClick(event)
{
  // we only care about left button events
  if (event.button != 0)
    return;

  var row = dirTree.treeBoxObject.getRowAt(event.clientX, event.clientY);
  if (row == -1 || row > dirTree.view.rowCount-1) {
    // double clicking on a non valid row should not open the dir properties dialog
    return;
  }

  if (dirTree && dirTree.view.selection && dirTree.view.selection.count == 1)
    AbEditSelectedDirectory();
}

function DirPaneSelectionChange()
{
  // clear out the search box when changing folders...
  onClearSearch();
  if (dirTree && dirTree.view.selection && dirTree.view.selection.count == 1) {
    gPreviousDirTreeIndex = dirTree.currentIndex;
    ChangeDirectoryByURI(GetSelectedDirectory());
  }
  goUpdateCommand('cmd_newlist');
}

function GetAbResultsBoxObject()
{
  if (!gAbResultsTree)
    gAbResultsTree = document.getElementById('abResultsTree');

  return gAbResultsTree.treeBoxObject;
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

function SetAbView(uri, searchView, sortColumn, sortDirection)
{
  var actualSortColumn;
  
  // make sure sortColumn and sortDirection have non null values before calling gAbView.init
  if (!sortColumn)
    sortColumn = kDefaultSortColumn;

  if (!sortDirection)
    sortDirection = kDefaultAscending;

  if (gAbView && gCurDirectory == GetSelectedDirectory())
  {
    // re-init the view
    actualSortColumn = gAbView.init(uri, searchView, GetAbViewListener(), sortColumn, sortDirection);
  }
  else
  {
    CloseAbView();

    gCurDirectory = GetSelectedDirectory();
    gAbView = Components.classes["@mozilla.org/addressbook/abview;1"].createInstance(Components.interfaces.nsIAbView);

    actualSortColumn = gAbView.init(uri, searchView, GetAbViewListener(), sortColumn, sortDirection);
  }

  var boxObject = GetAbResultsBoxObject();
  boxObject.view = gAbView.QueryInterface(Components.interfaces.nsITreeView);

  UpdateSortIndicators(sortColumn, sortDirection);
  
  return actualSortColumn;
}

function GetAbView()
{
  return gAbView;
}

function ChangeDirectoryByURI(uri)
{
  if (!uri)
    uri = kPersonalAddressbookURI;

  if (gAbView && gAbView.URI == uri)
    return;
  
  var sortColumn = gAbResultsTree.getAttribute("sortCol");
  var sortDirection = document.getElementById(sortColumn).getAttribute("sortDirection");
  
  var actualSortColumn = SetAbView(uri, false, sortColumn, sortDirection);

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
  var sortColumn = gAbResultsTree.getAttribute("sortCol");
  SortAndUpdateIndicators(sortColumn, kDefaultAscending);
}

function AbSortDescending()
{
  var sortColumn = gAbResultsTree.getAttribute("sortCol");
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
}

function UpdateSortIndicators(colID, sortDirection)
{
  var sortedColumn;
  // set the sort indicator on the column we are sorted by
  if (colID) {
    sortedColumn = document.getElementById(colID);
    if (sortedColumn) {
      sortedColumn.setAttribute("sortDirection",sortDirection);
      gAbResultsTree.setAttribute("sortCol", colID);
    }
  }

  // remove the sort indicator from all the columns
  // except the one we are sorted by
  var currCol = gAbResultsTree.firstChild.firstChild;
  while (currCol) {
    if (currCol != sortedColumn && currCol.localName == "treecol")
      currCol.removeAttribute("sortDirection");
    currCol = currCol.nextSibling;
  }
}

function InvalidateResultsPane()
{
  if (gAbResultsTree)
    gAbResultsTree.treeBoxObject.invalidate();
}

function AbNewList()
{
  goNewListDialog(GetSelectedDirectory());
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
    email = directory.description || card.displayName;
  }
  else 
    email = card.primaryEmail;

  return gHeaderParser.makeFullAddressWString(card.displayName, email);
}

function GetDirectoryFromURI(uri)
{
  return rdf.GetResource(uri).QueryInterface(Components.interfaces.nsIAbDirectory);
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
  if (abList)
    return abList.selectedItem.id;
  else {
    if (dirTree.currentIndex < 0)
      return null;
    var selected = dirTree.builderView.getResourceAtIndex(dirTree.currentIndex)
    return selected.Value;
  }
}

function onAbSearchKeyPress(event)
{
  // 13 == return
  if (event && event.keyCode == 13) 
    onAbSearchInput(true);
}
    
function onAbSearchInput(returnKeyHit)
{
  if (gSearchInput.showingSearchCriteria && !(returnKeyHit && gSearchInput.value == ""))
    return;

  SearchInputChanged();

  if (gSearchTimer) {
    clearTimeout(gSearchTimer);
    gSearchTimer = null;
  }

  if (returnKeyHit) {
    gSearchInput.select();
    onEnterInSearchBar();
  }
  else {
    gSearchTimer = setTimeout("onEnterInSearchBar();", 800);
  }
}

function SearchInputChanged() 
{
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
  if (gSearchInput) 
    gSearchInput.value ="";  //on input does not get fired for some reason
  onAbSearchInput(true);
}

function AbSwapFirstNameLastName()
{
  if (gAbView)
    gAbView.swapFirstNameLastName();
}


function onSearchInputFocus(event)
{
  // search bar has focus, ...clear the showing search criteria flag
  if (gSearchInput.showingSearchCriteria)
  {
    gSearchInput.value = "";
    gSearchInput.showingSearchCriteria = false;
  }

  gSearchInput.select();
}

// sets focus into the quick search box
function QuickSearchFocus()
{
  gSearchInput.focus();
}

function onSearchInputBlur(event)
{ 
//  if (gQuickSearchFocusEl && gQuickSearchFocusEl.id == 'searchInput') // ignore the blur if we are in the middle of processing the clear button
//    return;

  if (!gSearchInput.value)
    gSearchInput.showingSearchCriteria = true;
    
  if (gSearchInput.showingSearchCriteria)
    gSearchInput.setSearchCriteriaText();
}

var gQuickSearchFocusEl = null; 

function onClearSearch()
{
  if (gSearchInput && !gSearchInput.showingSearchCriteria) // ignore the text box value if it's just showing the search criteria string
  {
     onAbClearSearch();
     // this needs to be on a timer otherwise we end up messing up the focus while the Search("") is still happening
     setTimeout("restoreSearchFocusAfterClear();", 0); 
  }
}

function restoreSearchFocusAfterClear()
{
//  gQuickSearchFocusEl.focus();
  gSearchInput.clearButtonHidden = 'true';
  gQuickSearchFocusEl = null;
}

var gIsOffline;
var gSessionAdded;
var gCurrentAutocompleteDirectory;
var gAutocompleteSession;
var gSetupLdapAutocomplete;
var gLDAPSession;

function setupLdapAutocompleteSession()
{
    var autocompleteLdap = false;
    var autocompleteDirectory = null;
    var prevAutocompleteDirectory = gCurrentAutocompleteDirectory;
    var i;

    autocompleteLdap = gPrefs.getBoolPref("ldap_2.autoComplete.useDirectory");
    if (autocompleteLdap)
        autocompleteDirectory = gPrefs.getCharPref(
            "ldap_2.autoComplete.directoryServer");


    // use a temporary to do the setup so that we don't overwrite the
    // global, then have some problem and throw an exception, and leave the
    // global with a partially setup session.  we'll assign the temp
    // into the global after we're done setting up the session
    //
    var LDAPSession;
    if (gLDAPSession) {
        LDAPSession = gLDAPSession;
    } else {
        LDAPSession = Components.classes[
            "@mozilla.org/autocompleteSession;1?type=ldap"].createInstance()
            .QueryInterface(Components.interfaces.nsILDAPAutoCompleteSession);
    }
            
    if (autocompleteDirectory && !gIsOffline) { 
        // the compose window code adds an observer on the directory server
        // prefs, but I don't think we need this here.
        gCurrentAutocompleteDirectory = autocompleteDirectory;
        
        // fill in the session params if there is a session
        //
        if (LDAPSession) {
            var serverURL = Components.classes[
                "@mozilla.org/network/ldap-url;1"].
                createInstance().QueryInterface(
                    Components.interfaces.nsILDAPURL);

            try {
                serverURL.spec = gPrefs.getComplexValue(autocompleteDirectory +".uri",
                                           Components.interfaces.nsISupportsString).data;
            } catch (ex) {
                dump("ERROR: " + ex + "\n");
            }
            LDAPSession.serverURL = serverURL;

            // get the login to authenticate as, if there is one
            //
            var login = "";
            try {
                login = gPrefs.getComplexValue(
                    autocompleteDirectory + ".auth.dn",
                    Components.interfaces.nsISupportsString).data;
            } catch (ex) {
                // if we don't have this pref, no big deal
            }

            // find out if we need to authenticate, and if so, tell the LDAP
            // autocomplete session how to prompt for a password.  This window
            // (the compose window) is being used to parent the authprompter.
            //
            LDAPSession.login = login;
            if (login != "") {
                var windowWatcherSvc = Components.classes[
                    "@mozilla.org/embedcomp/window-watcher;1"]
                    .getService(Components.interfaces.nsIWindowWatcher);
                var domWin = 
                    window.QueryInterface(Components.interfaces.nsIDOMWindow);
                var authPrompter = 
                    windowWatcherSvc.getNewAuthPrompter(domWin);

                LDAPSession.authPrompter = authPrompter;
            }

            // don't search on non-CJK strings shorter than this
            //
            try { 
                LDAPSession.minStringLength = gPrefs.getIntPref(
                    autocompleteDirectory + ".autoComplete.minStringLength");
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default.
            }

            // don't search on CJK strings shorter than this
            //
            try { 
                LDAPSession.cjkMinStringLength = gPrefs.getIntPref(
                  autocompleteDirectory + ".autoComplete.cjkMinStringLength");
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default.
            }

            // we don't try/catch here, because if this fails, we're outta luck
            //
            var ldapFormatter = Components.classes[
                "@mozilla.org/ldap-autocomplete-formatter;1?type=addrbook"]
                .createInstance().QueryInterface(
                    Components.interfaces.nsIAbLDAPAutoCompFormatter);

            // override autocomplete name format?
            //
            try {
                ldapFormatter.nameFormat = 
                    gPrefs.getComplexValue(autocompleteDirectory + 
                                      ".autoComplete.nameFormat",
                                      Components.interfaces.nsISupportsString).data;
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsAbLDAPAutoCompFormatter use its default.
            }

            // override autocomplete mail address format?
            //
            try {
                ldapFormatter.addressFormat = 
                    gPrefs.getComplexValue(autocompleteDirectory + 
                                      ".autoComplete.addressFormat",
                                      Components.interfaces.nsISupportsString).data;
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsAbLDAPAutoCompFormatter use its default.
            }

            try {
                // figure out what goes in the comment column, if anything
                //
                // 0 = none
                // 1 = name of addressbook this card came from
                // 2 = other per-addressbook format
                //
                var showComments = 0;
                showComments = gPrefs.getIntPref(
                    "mail.autoComplete.commentColumn");

                switch (showComments) {

                case 1:
                    // use the name of this directory
                    //
                    ldapFormatter.commentFormat = gPrefs.getComplexValue(
                                autocompleteDirectory + ".description",
                                Components.interfaces.nsISupportsString).data;
                    break;

                case 2:
                    // override ldap-specific autocomplete entry?
                    //
                    try {
                        ldapFormatter.commentFormat = 
                            gPrefs.getComplexValue(autocompleteDirectory + 
                                        ".autoComplete.commentFormat",
                                        Components.interfaces.nsISupportsString).data;
                    } catch (innerException) {
                        // if nothing has been specified, use the ldap
                        // organization field
                        ldapFormatter.commentFormat = "[o]";
                    }
                    break;

                case 0:
                default:
                    // do nothing
                }
            } catch (ex) {
                // if something went wrong while setting up comments, try and
                // proceed anyway
            }

            // set the session's formatter, which also happens to
            // force a call to the formatter's getAttributes() method
            // -- which is why this needs to happen after we've set the
            // various formats
            //
            LDAPSession.formatter = ldapFormatter;

            // override autocomplete entry formatting?
            //
            try {
                LDAPSession.outputFormat = 
                    gPrefs.getComplexValue(autocompleteDirectory + 
                                      ".autoComplete.outputFormat",
                                      Components.interfaces.nsISupportsString).data;

            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default.
            }

            // override default search filter template?
            //
            try { 
                LDAPSession.filterTemplate = gPrefs.getComplexValue(
                    autocompleteDirectory + ".autoComplete.filterTemplate",
                    Components.interfaces.nsISupportsString).data;

            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default
            }

            // override default maxHits (currently 100)
            //
            try { 
                // XXXdmose should really use .autocomplete.maxHits,
                // but there's no UI for that yet
                // 
                LDAPSession.maxHits = 
                    gPrefs.getIntPref(autocompleteDirectory + ".maxHits");
            } catch (ex) {
                // if this pref isn't there, or is out of range, no big deal. 
                // just let nsLDAPAutoCompleteSession use its default.
            }

            if (!gSessionAdded) {
                // if we make it here, we know that session initialization has
                // succeeded; add the session for all recipients, and 
                // remember that we've done so
                var autoCompleteWidget;
                for (i=1; i <= awGetMaxRecipients(); i++)
                {
                    autoCompleteWidget = document.getElementById("addressCol1#" + i);
                    if (autoCompleteWidget)
                    {
                      autoCompleteWidget.addSession(LDAPSession);
                      // ldap searches don't insert a default entry with the default domain appended to it
                      // so reduce the minimum results for a popup to 2 in this case. 
                      autoCompleteWidget.minResultsForPopup = 2;

                    }
                 }
                gSessionAdded = true;
            }
        }
    } else {
      if (gCurrentAutocompleteDirectory) {
        gCurrentAutocompleteDirectory = null;
      }
      if (gLDAPSession && gSessionAdded) {
        for (i=1; i <= awGetMaxRecipients(); i++) 
          document.getElementById("addressCol1#" + i).
              removeSession(gLDAPSession);
        gSessionAdded = false;
      }
    }

    gLDAPSession = LDAPSession;
    gSetupLdapAutocomplete = true;
}
