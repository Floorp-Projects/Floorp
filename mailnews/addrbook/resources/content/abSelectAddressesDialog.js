/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Hangas <hangas@netscape.com>
 *   Alec Flett <alecf@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var addressbook = 0;
var composeWindow = 0;
var msgCompFields = 0;
var editCardCallback = 0;

var gAddressBookBundle;

var gSearchInput;
var gSearchTimer = null;
var gQueryURIFormat = null;

// localization strings
var prefixTo;
var prefixCc;
var prefixBcc;

var gToButton;
var gCcButton;
var gBccButton;

var gActivatedButton;

var gDragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
gDragService = gDragService.QueryInterface(Components.interfaces.nsIDragService);

var gSelectAddressesAbViewListener = {
	onSelectionChanged: function() {
    ResultsPaneSelectionChanged();
  },
  onCountChanged: function(total) {
    // do nothing
  }
};

function GetAbViewListener()
{
  return gSelectAddressesAbViewListener;
}

function OnLoadSelectAddress()
{
  gAddressBookBundle = document.getElementById("bundle_addressBook");
  prefixTo = gAddressBookBundle.getString("prefixTo") + ": ";
  prefixCc = gAddressBookBundle.getString("prefixCc") + ": ";
  prefixBcc = gAddressBookBundle.getString("prefixBcc") + ": ";

  InitCommonJS();

  UpgradeAddressBookResultsPaneUI("mailnews.ui.select_addresses_results.version");

  var toAddress="", ccAddress="", bccAddress="";

  doSetOKCancel(SelectAddressOKButton, 0);

  top.addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);

  // look in arguments[0] for parameters
  if (window.arguments && window.arguments[0])
  {
    // keep parameters in global for later
    if ( window.arguments[0].composeWindow )
      top.composeWindow = window.arguments[0].composeWindow;
    if ( window.arguments[0].msgCompFields )
      top.msgCompFields = window.arguments[0].msgCompFields;
    if ( window.arguments[0].toAddress )
      toAddress = window.arguments[0].toAddress;
    if ( window.arguments[0].ccAddress )
      ccAddress = window.arguments[0].ccAddress;
    if ( window.arguments[0].bccAddress )
      bccAddress = window.arguments[0].bccAddress;

    // put the addresses into the bucket
    AddAddressFromComposeWindow(toAddress, prefixTo);
    AddAddressFromComposeWindow(ccAddress, prefixCc);
    AddAddressFromComposeWindow(bccAddress, prefixBcc);
  }

  gSearchInput = document.getElementById("searchInput");
  SearchInputChanged();

  SelectFirstAddressBookMenulist();

  DialogBucketPaneSelectionChanged();
  
  var workPhoneCol = document.getElementById("WorkPhone");
  workPhoneCol.setAttribute("hidden", "true");
  
  var companyCol = document.getElementById("Company");
  companyCol.setAttribute("hidden", "true");

  gToButton = document.getElementById("toButton");
  gCcButton = document.getElementById("ccButton");
  gBccButton = document.getElementById("bccButton");

  var abResultsTree = document.getElementById("abResultsTree");
  abResultsTree.focus();

  gActivatedButton = gToButton;

  document.documentElement.addEventListener("keypress", OnReturnHit, true);
}

function OnUnloadSelectAddress()
{
  CloseAbView();
}

function AddAddressFromComposeWindow(addresses, prefix)
{
  if ( addresses )
  {
    var emails = {};
    var names = {};
    var fullNames = {};
    var numAddresses = gHeaderParser.parseHeadersWithArray(addresses, emails, names, fullNames);

    for ( var index = 0; index < numAddresses; index++ )
    {
      AddAddressIntoBucket(prefix, fullNames.value[index], emails.value[index]);
    }
  }
}

function SelectAddressOKButton()
{
  var body = document.getElementById('bucketBody');
  var item, row, cell, prefix, address, email;
  var toAddress="", ccAddress="", bccAddress="", emptyEmail="";

  for ( var index = 0; index < body.childNodes.length; index++ )
  {
    item = body.childNodes[index];
    if ( item.childNodes && item.childNodes.length )
    {
      row = item.childNodes[0];
      if (  row.childNodes &&  row.childNodes.length )
      {
        cell = row.childNodes[0];
        prefix = cell.getAttribute('prefix');
        address = cell.getAttribute('address');
        email = cell.getAttribute('email');
        if ( prefix )
        {
          switch ( prefix )
          {
            case prefixTo:
              if ( toAddress )
                toAddress += ", ";
              toAddress += address;
              break;
            case prefixCc:
              if ( ccAddress )
                ccAddress += ", ";
              ccAddress += address;
              break;
            case prefixBcc:
              if ( bccAddress )
                bccAddress += ", ";
              bccAddress += address;
              break;
          }
        }
        if(!email)
        {
          if (emptyEmail)
            emptyEmail +=", ";
            emptyEmail += text.substring(prefixTo.length, text.length-2);
        }
      }
    }
  }
  if(emptyEmail)
  {
    var alertText = gAddressBookBundle.getString("emptyEmailCard");
    alert(alertText + emptyEmail);
    return false;
  }
  // reset the UI in compose window
  msgCompFields.to = toAddress;
  msgCompFields.cc = ccAddress;
  msgCompFields.bcc = bccAddress;
  top.composeWindow.CompFields2Recipients(top.msgCompFields);

  return true;
}

function SelectAddressToButton()
{
  AddSelectedAddressesIntoBucket(prefixTo);
  gActivatedButton = gToButton;
}

function SelectAddressCcButton()
{
  AddSelectedAddressesIntoBucket(prefixCc);
  gActivatedButton = gCcButton;
}

function SelectAddressBccButton()
{
  AddSelectedAddressesIntoBucket(prefixBcc);
  gActivatedButton = gBccButton;
}

function AddSelectedAddressesIntoBucket(prefix)
{
  var cards = GetSelectedAbCards();
  var count = cards.length;

  for (var i = 0; i < count; i++) {
    AddCardIntoBucket(prefix, cards[i]);
  }
}

function AddCardIntoBucket(prefix, card)
{
  var address = GenerateAddressFromCard(card);
  if (card.isMailList) {
    AddAddressIntoBucket(prefix, address, card.displayName);
    }
  else {
    AddAddressIntoBucket(prefix, address, card.primaryEmail);
  }
}

function AddAddressIntoBucket(prefix, address, email)
{
  var body = document.getElementById("bucketBody");

  var item = document.createElement('treeitem');
  var row = document.createElement('treerow');
  var cell = document.createElement('treecell');
  cell.setAttribute('label', prefix + address);
  cell.setAttribute('prefix', prefix);
  cell.setAttribute('address', address);
  cell.setAttribute('email', email);

  row.appendChild(cell);
  item.appendChild(row);
  body.appendChild(item);
}

function RemoveSelectedFromBucket()
{
  var bucketTree = document.getElementById("addressBucket");
  if ( bucketTree )
  {
    var body = document.getElementById("bucketBody");
    var selection = bucketTree.view.selection;
    var rangeCount = selection.getRangeCount();

    for (var i = rangeCount-1; i >= 0; --i)
    {
      var start = {}, end = {};
      selection.getRangeAt(i,start,end);
      for (var j = end.value; j >= start.value; --j)
      {
        var item = bucketTree.contentView.getItemAtIndex(j);
        body.removeChild(item);
      }
    }
  }
}

/* Function: ResultsPaneSelectionChanged()
 * Callers : OnLoadSelectAddress(), abCommon.js:ResultsPaneSelectionChanged()
 * -------------------------------------------------------------------------
 * This function is used to grab the selection state of the results tree to maintain
 * the appropriate enabled/disabled states of the "Edit", "To:", "CC:", and "Bcc:" buttons.
 * If an entry is selected in the results Tree, then the "disabled" attribute is removed.
 * Otherwise, if nothing is selected, "disabled" is set to true.
 */

function ResultsPaneSelectionChanged()
{;
  var editButton = document.getElementById("edit");
  var toButton = document.getElementById("toButton");
  var ccButton = document.getElementById("ccButton");
  var bccButton = document.getElementById("bccButton");

  var numSelected = GetNumSelectedCards();
  if (numSelected > 0)
  {
    if (numSelected == 1)
    editButton.removeAttribute("disabled");
    else
      editButton.setAttribute("disabled", "true");

    toButton.removeAttribute("disabled");
    ccButton.removeAttribute("disabled");
    bccButton.removeAttribute("disabled");
  }
  else
  {
    editButton.setAttribute("disabled", "true");
    toButton.setAttribute("disabled", "true");
    ccButton.setAttribute("disabled", "true");
    bccButton.setAttribute("disabled", "true");
  }
}

/* Function: DialogBucketPaneSelectionChanged()
 * Callers : OnLoadSelectAddress(), abSelectAddressesDialog.xul:id="addressBucket"
 * -------------------------------------------------------------------------------
 * This function is used to grab the selection state of the bucket tree to maintain
 * the appropriate enabled/disabled states of the "Remove" button.
 * If an entry is selected in the bucket Tree, then the "disabled" attribute is removed.
 * Otherwise, if nothing is selected, "disabled" is set to true.
 */

function DialogBucketPaneSelectionChanged()
{
  var bucketTree = document.getElementById("addressBucket");
  var removeButton = document.getElementById("remove");

  removeButton.disabled = bucketTree.view.selection.count == 0;
}

function AbResultsPaneDoubleClick(card)
{
  AddCardIntoBucket(prefixTo, card);
}

function OnClickedCard(card)
{
  // in the select address dialog, do nothing on click
}

function UpdateCardView()
{
  // in the select address dialog, do nothing
}

function DragOverBucketPane(event)
{
  var dragSession = gDragService.getCurrentSession();

  if (dragSession.isDataFlavorSupported("text/x-moz-address"))
    dragSession.canDrop = true;
}

function DropOnBucketPane(event)
{
  var dragSession = gDragService.getCurrentSession();
  var trans;
  
  try {
    trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
    trans.addDataFlavor("text/x-moz-address");
  }
  catch (ex) {
    return;
  }

  for ( var i = 0; i < dragSession.numDropItems; ++i )
  {
    dragSession.getData ( trans, i );
    var dataObj = new Object();
    var bestFlavor = new Object();
    var len = new Object();
    trans.getAnyTransferData ( bestFlavor, dataObj, len );
    if ( dataObj )  
      dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
    if ( !dataObj ) 
      continue;

    // pull the address out of the data object
    var address = dataObj.data.substring(0, len.value);
    if (!address)
      continue;

    AddAddressIntoBucket(prefixTo, address, address);
  }
}

function OnReturnHit(event)
{  
  if (event.keyCode == 13) {
    var focusedElement = document.commandDispatcher.focusedElement;
    if (focusedElement && (focusedElement.id == "addressBucket"))
      return;
    event.preventBubble();
    if (focusedElement && (focusedElement.id == "abResultsTree"))
      gActivatedButton.doCommand();
  }
}


function onEnterInSearchBar()
{
  var selectedNode = abList.selectedItem;
 
  if (!selectedNode)
    return;

  if (!gQueryURIFormat) {
    gQueryURIFormat = gPrefs.getComplexValue("mail.addr_book.quicksearchquery.format", 
                                              Components.interfaces.nsIPrefLocalizedString).data;
  }
  
  var sortColumn = selectedNode.getAttribute("sortColumn");
  var sortDirection = selectedNode.getAttribute("sortDirection");
  var searchURI = selectedNode.getAttribute("id");

  if (gSearchInput.value != "") {
    searchURI += gQueryURIFormat.replace(/@V/g, encodeURIComponent(gSearchInput.value));
  }

  if (!sortColumn.Length)
    sortColumn = "GeneratedName";
  SetAbView(searchURI, true, sortColumn, sortDirection);
  
  SelectFirstCard();
}

function SelectFirstAddressBookMenulist()
{
  ChangeDirectoryByURI(abList.selectedItem.id);
  return;
}

function DirPaneSelectionChangeMenulist()
{
  if (abList && abList.selectedItem) {
    if (gSearchInput.value && (gSearchInput.value != "")) 
      onEnterInSearchBar();
    else
      ChangeDirectoryByURI(abList.selectedItem.id);
  }
}
