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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

top.MAX_RECIPIENTS = 1;
var inputElementType = "";

var gParentURI;
var gListCard;
var gEditList;
var gOkCallback = null;
var oldListName = "";
var gAddressBookBundle;
var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
var gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
var gHeaderParser = Components.classes["@mozilla.org/messenger/headerparser;1"].getService(Components.interfaces.nsIMsgHeaderParser);

try
{
  var gDragService = Components.classes["@mozilla.org/widget/dragservice;1"]
                               .getService(Components.interfaces.nsIDragService);
}
catch (e)
{
}

function handleKeyPress(element, event)
{
  // allow dialog to close on enter if focused textbox has no value
  if (element.value != "" && event.keyCode == 13)
    event.preventBubble();
}

function mailingListExists(listname)
{
  var addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);
  if (addressbook.mailListNameExists(listname))
  {
    gPromptService.alert(window, 
      gAddressBookBundle.getString("mailListNameExistsTitle"),
      gAddressBookBundle.getString("mailListNameExistsMessage"));
    return true;
  }
  return false;
}

function GetListValue(mailList, doAdd)
{
  var listname = document.getElementById('ListName').value;

  if (listname.length == 0)
  {
    var alertText = gAddressBookBundle.getString("emptyListName");
    alert(alertText);
    return false;
  }
  else
  {
    var canonicalNewListName = listname.toLowerCase();
    var canonicalOldListName = oldListName.toLowerCase();
    if (doAdd)
    {
      if (mailingListExists(canonicalNewListName))
        return false;
    }
    else if (canonicalOldListName != canonicalNewListName)
    {
      if (mailingListExists(canonicalNewListName))
        return false;
    }
  }

  mailList.dirName = listname;
  mailList.listNickName = document.getElementById('ListNickName').value;
  mailList.description = document.getElementById('ListDescription').value;

  var oldTotal = mailList.addressLists.Count();
  var i = 1;
  var pos = 0;
  var inputField, fieldValue, cardproperty;
  while ((inputField = awGetInputElement(i)))
  {

    fieldValue = inputField.value;
    
    if (doAdd || (!doAdd && pos >= oldTotal))
      cardproperty = Components.classes["@mozilla.org/addressbook/cardproperty;1"].createInstance();
    else
      cardproperty = mailList.addressLists.GetElementAt(pos);

    if (fieldValue == "")
    {
      if (!doAdd && cardproperty)
      try
      {
        mailList.addressLists.DeleteElementAt(pos);
      }
      catch(ex)
      {
        // Ignore attempting to remove an item
        // at a position greater than the number
        // of elements in the addressLists attribute
      }
    }
    else if (cardproperty)
    {
      cardproperty = cardproperty.QueryInterface(Components.interfaces.nsIAbCard);
      if (cardproperty)
      {
        var addresses = {};
        var names = {};
        var fullNames = {};
        var numAddresses = gHeaderParser.parseHeadersWithArray(fieldValue, addresses, names, fullNames);

        cardproperty.primaryEmail = addresses.value[0];
        cardproperty.displayName = names.value[0];

        if (doAdd || (doAdd == false && pos >= oldTotal))
          mailList.addressLists.AppendElement(cardproperty);
        pos++;
      }
    }
    i++;
  }

  --i;
  
  if (doAdd == false && i < oldTotal)
  {
    for (var j = i; j < oldTotal; j++)
      mailList.addressLists.DeleteElementAt(j);
  }
  return true;
}

function MailListOKButton()
{
  var popup = document.getElementById('abPopup');
  if ( popup )
  {
    var uri = popup.getAttribute('value');

    // FIX ME - hack to avoid crashing if no ab selected because of blank option bug from template
    // should be able to just remove this if we are not seeing blank lines in the ab popup
    if ( !uri )
      return false;  // don't close window
    // -----

    //Add mailing list to database
    var mailList = Components.classes["@mozilla.org/addressbook/directoryproperty;1"].createInstance();
    mailList = mailList.QueryInterface(Components.interfaces.nsIAbDirectory);

    if (GetListValue(mailList, true)) {
       var parentDirectory = GetDirectoryFromURI(uri);
       parentDirectory.addMailList(mailList);
    }
    else
      return false;
  }

  return true;  // close the window
}

function OnLoadNewMailList()
{
  //XXX: gAddressBookBundle is set in 2 places because of different callers
  gAddressBookBundle = document.getElementById("bundle_addressBook");
  var selectedAB = null;

  if (window.arguments && window.arguments[0])
  {
    var abURI = window.arguments[0].selectedAB;
    if (abURI) {
      var directory = GetDirectoryFromURI(abURI);
      if (directory.isMailList) {
        var parentURI = GetParentDirectoryFromMailingListURI(abURI);
        if (parentURI) {
          selectedAB = parentURI;
        }
  }
      else if (!(directory.operations & directory.opWrite)) {
        selectedAB = kPersonalAddressbookURI;
        
      }      
      else {
        selectedAB = abURI;
      }
    }
  }
  
  if (!selectedAB)
    selectedAB = kPersonalAddressbookURI;

  // set popup with address book names
  var abPopup = document.getElementById('abPopup');
  abPopup.value = selectedAB;

  AppendNewRowAndSetFocus();
  awFitDummyRows(1);

  document.addEventListener("keypress", awDocumentKeyPress, true);

  // focus on first name
  var listName = document.getElementById('ListName');
  if ( listName )
    setTimeout( function(firstTextBox) { firstTextBox.focus(); }, 0, listName );

  moveToAlertPosition();
}

function EditListOKButton()
{
  //edit mailing list in database
  if (GetListValue(gEditList, false))
  {
    if (gListCard) {
      // modify the list card (for the results pane) from the mailing list 
      gListCard.displayName = gEditList.dirName;
      gListCard.lastName = gEditList.dirName;
      gListCard.nickName = gEditList.listNickName;
      gListCard.notes = gEditList.description;
    }

    gEditList.editMailListToDatabase(gParentURI, gListCard);

    if (gOkCallback)
      gOkCallback();
    return true;  // close the window
  }
  else
    return false;
}

function OnLoadEditList()
{
  //XXX: gAddressBookBundle is set in 2 places because of different callers
  gAddressBookBundle = document.getElementById("bundle_addressBook");

  gParentURI  = window.arguments[0].abURI;
  gListCard = window.arguments[0].abCard;
  var listUri  = window.arguments[0].listURI;
  gOkCallback = window.arguments[0].okCallback;

  gEditList = GetDirectoryFromURI(listUri);

  document.getElementById('ListName').value = gEditList.dirName;
  document.getElementById('ListNickName').value = gEditList.listNickName;
  document.getElementById('ListDescription').value = gEditList.description;
  oldListName = gEditList.dirName;

  if (gEditList.addressLists)
  {
    var total = gEditList.addressLists.Count();
    if (total)
    {
      var listbox = document.getElementById('addressingWidget');
      var newListBoxNode = listbox.cloneNode(false);
      var templateNode = listbox.getElementsByTagName("listitem")[0];

      top.MAX_RECIPIENTS = 0;
      for ( var i = 0;  i < total; i++ )
      {
        var card = gEditList.addressLists.GetElementAt(i);
        card = card.QueryInterface(Components.interfaces.nsIAbCard);
        var address = gHeaderParser.makeFullAddressWString(card.displayName, card.primaryEmail);
        SetInputValue(address, newListBoxNode, templateNode);
      }
      var parent = listbox.parentNode;
      parent.replaceChild(newListBoxNode, listbox);
    }
  }

  document.addEventListener("keypress", awDocumentKeyPress, true);

  moveToAlertPosition();

  // workaround for bug 118337 - for mailing lists that have more rows than fits inside
  // the display, the value of the textbox inside the new row isn't inherited into the input -
  // the first row then appears to be duplicated at the end although it is actually empty.
  // see awAppendNewRow which copies first row and clears it
  setTimeout(AppendLastRow, 0);
}

function AppendLastRow()
{ 
  AppendNewRowAndSetFocus();
  awFitDummyRows(1);

  // focus on first name
  var listName = document.getElementById('ListName');
  if ( listName )
    listName.focus();
}

function AppendNewRowAndSetFocus()
{
  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
  if ( lastInput && lastInput.value )
    awAppendNewRow(true);
  else
    awSetFocus(top.MAX_RECIPIENTS, lastInput);
}

function SetInputValue(inputValue, parentNode, templateNode)
{
    top.MAX_RECIPIENTS++;

    var newNode = templateNode.cloneNode(true);
    parentNode.appendChild(newNode); // we need to insert the new node before we set the value of the select element!

    var input = newNode.getElementsByTagName(awInputElementName());
    if ( input && input.length == 1 )
    {
    //We need to set the value using both setAttribute and .value else we will
    // lose the content when the field is not visible. See bug 37435
      input[0].setAttribute("value", inputValue);
      input[0].value = inputValue;
      input[0].setAttribute("id", "addressCol1#" + top.MAX_RECIPIENTS);
  }
}

function awNotAnEmptyArea(event)
{
  //This is temporary until i figure out how to ensure to always having an empty space after the last row

  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
  if ( lastInput && lastInput.value )
    awAppendNewRow(false);

  event.preventBubble();
}

function awClickEmptySpace(target, setFocus)
{
  if (target == null ||
      (target.localName != "listboxbody" &&
      target.localName != "listcell" &&
      target.localName != "listitem"))
    return;

  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);

  if ( lastInput && lastInput.value )
    awAppendNewRow(setFocus);
  else
    if (setFocus)
      awSetFocus(top.MAX_RECIPIENTS, lastInput);
}

function awReturnHit(inputElement)
{
  var row = awGetRowByInputElement(inputElement);
  if ( inputElement.value )
  {
    var nextInput = awGetInputElement(row+1);
    if ( !nextInput )
      awAppendNewRow(true);
    else
      awSetFocus(row+1, nextInput);
  }
}

function awInputChanged(inputElement)
{
//  AutoCompleteAddress(inputElement);

  //Do we need to add a new row?
  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
  if ( lastInput && lastInput.value && !top.doNotCreateANewRow)
    awAppendNewRow(false);
  top.doNotCreateANewRow = false;
}

function awInputElementName()
{
    if (inputElementType == "")
        inputElementType = document.getElementById("addressCol1#1").localName;
    return inputElementType;
}

function awAppendNewRow(setFocus)
{
  var body = document.getElementById("addressingWidget");
  var listitem1 = awGetListItem(1);

  if ( body && listitem1 )
  {  
    var nextDummy = awGetNextDummyRow();
    var newNode = listitem1.cloneNode(true);
    if (nextDummy)
      body.replaceChild(newNode, nextDummy);
    else
      body.appendChild(newNode);
    
    top.MAX_RECIPIENTS++;

    var input = newNode.getElementsByTagName(awInputElementName());
    if ( input && input.length == 1 )
    {
      input[0].setAttribute("value", "");
      input[0].setAttribute("id", "addressCol1#" + top.MAX_RECIPIENTS);
      
      if (input[0].getAttribute('focused') != '')
        input[0].removeAttribute('focused');
    }
    // focus on new input widget
    if (setFocus && input )
      awSetFocus(top.MAX_RECIPIENTS, input[0]);
  }
}


// functions for accessing the elements in the addressing widget

function awGetInputElement(row)
{
    return document.getElementById("addressCol1#" + row);
}


function _awSetFocus()
{
  var listbox = document.getElementById('addressingWidget');
  try
  {
    var theNewRow = awGetListItem(top.awRow);

    listbox.ensureElementIsVisible(theNewRow);
    top.awInputElement.focus();
  }
  catch(ex)
  {
    top.awFocusRetry ++;
    if (top.awFocusRetry < 8)
    {
      dump("_awSetFocus failed, try it again...\n");
      setTimeout("_awSetFocus();", 0);
    }
    else
      dump("_awSetFocus failed, forget about it!\n");
  }
}

function awTabFromRecipient(element, event)
{
  //If we are the last element in the listbox, we don't want to create a new row.
  if (element == awGetInputElement(top.MAX_RECIPIENTS))
    top.doNotCreateANewRow = true;
}

function DragOverAddressListTree(event)
{
  var validFlavor = false;
  var dragSession = gDragService.getCurrentSession();

  // XXX add support for other flavors here
  if (dragSession.isDataFlavorSupported("text/x-moz-address")) {
    dragSession.canDrop = true;
  }
}

function DropOnAddressListTree(event)
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
    if ( dataObj )  dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
    if ( !dataObj ) continue;

    // pull the URL out of the data object
    var address = dataObj.data.substring(0, len.value);
    if (!address)
      continue;

    DropListAddress(event.target, address);
  }
}

function DropListAddress(target, address)
{
    awClickEmptySpace(target, true);    //that will automatically set the focus on a new available row, and make sure is visible
    if (top.MAX_RECIPIENTS == 0)
    top.MAX_RECIPIENTS = 1;
  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
    lastInput.value = address;
    awAppendNewRow(true);
}
