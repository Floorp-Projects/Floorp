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
 *
 * Contributor(s):
 *   Paul Hangas <hangas@netscape.com>
 *   Alec Flett <alecf@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 */

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
    var addressArray = addresses.split(",");

    for ( var index = 0; index < addressArray.length; index++ )
    {
      // remove leading spaces
      while ( addressArray[index][0] == " " )
        addressArray[index] = addressArray[index].substring(1, addressArray[index].length);

      AddAddressIntoBucket(prefix + addressArray[index]);
    }
  }
}

function SelectAddressOKButton()
{
  var body = document.getElementById('bucketBody');
  var item, row, cell, text, colon,email;
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
        text = cell.getAttribute('label');
        email = cell.getAttribute('email');
        if ( text )
        {
          switch ( text[0] )
          {
            case prefixTo[0]:
              if ( toAddress )
                toAddress += ", ";
              toAddress += text.substring(prefixTo.length, text.length);
              break;
            case prefixCc[0]:
              if ( ccAddress )
                ccAddress += ", ";
              ccAddress += text.substring(prefixCc.length, text.length);
              break;
            case prefixBcc[0]:
              if ( bccAddress )
                bccAddress += ", ";
              bccAddress += text.substring(prefixBcc.length, text.length);
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
}

function SelectAddressCcButton()
{
  AddSelectedAddressesIntoBucket(prefixCc);
}

function SelectAddressBccButton()
{
  AddSelectedAddressesIntoBucket(prefixBcc);
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
  var address = prefix + GenerateAddressFromCard(card);
  if (card.isMailList) {
    AddAddressIntoBucket(address, card.displayName);
    }
  else {
    AddAddressIntoBucket(address, card.primaryEmail);
  }
}

function AddAddressIntoBucket(address,email)
{
  var body = document.getElementById("bucketBody");

  var item = document.createElement('treeitem');
  var row = document.createElement('treerow');
  var cell = document.createElement('treecell');
  cell.setAttribute('label', address);
  cell.setAttribute('email',email);

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

    for (i = rangeCount-1; i >= 0; --i)
    {
      var start = {}, end = {};
      selection.getRangeAt(i,start,end);
      for (j = end.value; j >= start.value; --j)
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

    AddAddressIntoBucket(prefixTo + address, address);
  }
}

function OnReturnHit(event)
{
  if (event.keyCode == 13 && (document.commandDispatcher.focusedElement == gSearchInput.inputField))
	event.preventBubble();
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
    searchURI += gQueryURIFormat.replace(/@V/g, escape(gSearchInput.value));
  }

  SetAbView(searchURI, sortColumn, sortDirection);
  
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
