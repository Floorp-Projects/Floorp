/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

top.MAX_RECIPIENTS = 1;
var inputElementType = "";

var mailList;
var parentURI;
var editList;
var hitReturnInList = false;
var oldListName = "";
var gAddressBookBundle;

function handleKeyPress(element, event)
{
  if (event.keyCode == 13)
  {
    hitReturnInList = true;
    awReturnHit(element);
  }
}

function mailingListExists(listname)
{
  var addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);
  if (addressbook.mailListNameExists(listname))
  {
    var alertText = gAddressBookBundle.getString("mailListNameExists");
    alert(alertText);
    return true;
  }
  return false;
}

function GetListValue(mailList, doAdd)
{
  mailList.listName = document.getElementById('ListName').value;

  if (mailList.listName.length == 0)
  {
    var alertText = gAddressBookBundle.getString("emptyListName");
    alert(alertText);
    return false;
  }
  else
  {
    var listname = mailList.listName;
    listname = listname.toLowerCase();
    oldListName = oldListName.toLowerCase();
    if (doAdd == true)
    {
      if (mailingListExists(listname))
        return false;
    }
    else if (oldListName != listname)
    {
      if (mailingListExists(listname))
        return false;
    }
  }

  mailList.listNickName = document.getElementById('ListNickName').value;
  mailList.description = document.getElementById('ListDescription').value;

  var oldTotal = mailList.addressLists.Count();
  var i = 1;
  var pos = 0;
  var inputField, fieldValue, cardproperty;
  while ((inputField = awGetInputElement(i)))
  {
          fieldValue = inputField.value;
    if (doAdd || (doAdd == false && pos >= oldTotal))
      cardproperty = Components.classes["@mozilla.org/addressbook/cardproperty;1"].createInstance();
    else
      cardproperty = mailList.addressLists.GetElementAt(pos);

    if (fieldValue == "")
    {
      if (doAdd == false && cardproperty)
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
        var beginpos = fieldValue.search('<');
        var endpos = fieldValue.search('>');
        if (beginpos != -1)
        {
          beginpos++;
          var newValue = fieldValue.slice(beginpos, endpos);
          cardproperty.primaryEmail = newValue;
        }
        else
          cardproperty.primaryEmail = fieldValue;
        if (doAdd || (doAdd == false && pos >= oldTotal))
          mailList.addressLists.AppendElement(cardproperty);
        pos++;
      }
    }
      i++;
  }
  if (doAdd == false && i < oldTotal)
  {
    for (var j = i; j < oldTotal; j++)
      mailList.addressLists.DeleteElementAt(j);
  }
  return true;
}

function MailListOKButton()
{
  if (hitReturnInList)
  {
    hitReturnInList = false;
    return false;
  }
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
    mailList = Components.classes["@mozilla.org/addressbook/directoryproperty;1"].createInstance();
    mailList = mailList.QueryInterface(Components.interfaces.nsIAbDirectory);

    if (GetListValue(mailList, true))
      mailList.addMailListToDatabase(uri);
    else
      return false;
  }
  return true;  // close the window
}

function OnLoadMailList()
{
  //XXX: gAddressBookBundle is set in 2 places because of different callers
  gAddressBookBundle = document.getElementById("bundle_addressBook");
  doSetOKCancel(MailListOKButton, 0);

  var selectedAB;
  if (window.arguments && window.arguments[0])
  {
    if ( window.arguments[0].selectedAB )
      selectedAB = window.arguments[0].selectedAB;
    else
      selectedAB = "moz-abmdbdirectory://abook.mab";
  }

  // set popup with address book names
  var abPopup = document.getElementById('abPopup');
  if ( abPopup )
  {
    var menupopup = document.getElementById('abPopup-menupopup');

    if ( selectedAB && menupopup && menupopup.childNodes )
    {
      for ( var index = menupopup.childNodes.length - 1; index >= 0; index-- )
      {
        if ( menupopup.childNodes[index].getAttribute('value') == selectedAB )
        {
          abPopup.label = menupopup.childNodes[index].getAttribute('label');
          abPopup.value = menupopup.childNodes[index].getAttribute('value');
          break;
        }
      }
    }
  }

  AppendnewRowAndSetFocus();

  // focus on first name
  var listName = document.getElementById('ListName');
  if ( listName )
    listName.focus();
  moveToAlertPosition();
}

function EditListOKButton()
{
  if (hitReturnInList)
  {
    hitReturnInList = false;
    return false;
  }
  //Add mailing list to database
  if (GetListValue(editList, false))
  {
    editList.editMailListToDatabase(parentURI);
    return true;  // close the window
  }
  else
    return false;
}

function OnLoadEditList()
{
  //XXX: gAddressBookBundle is set in 2 places because of different callers
  gAddressBookBundle = document.getElementById("bundle_addressBook");
  doSetOKCancel(EditListOKButton, 0);

  parentURI  = window.arguments[0].abURI;
  var listUri  = window.arguments[0].listURI;

  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
  rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
  editList = rdf.GetResource(listUri);
  editList = editList.QueryInterface(Components.interfaces.nsIAbDirectory);

  document.getElementById('ListName').value = editList.listName;
  document.getElementById('ListNickName').value = editList.listNickName;
  document.getElementById('ListDescription').value = editList.description;
  oldListName = editList.listName;

  if (editList.addressLists)
  {
    var total = editList.addressLists.Count();
    if (total)
    {
      var treeChildren = document.getElementById('addressList');
      var newTreeChildrenNode = treeChildren.cloneNode(false);
      var templateNode = treeChildren.firstChild;

      top.MAX_RECIPIENTS = 0;
      for ( var i = 0;  i < total; i++ )
      {
        var card = editList.addressLists.GetElementAt(i);
        card = card.QueryInterface(Components.interfaces.nsIAbCard);
        var address;
        if (card.name.length)
          address = card.name + " <" + card.primaryEmail + ">";
        else
          address = card.primaryEmail;
        SetInputValue(address, newTreeChildrenNode, templateNode);
      }
      var parent = treeChildren.parentNode;
      parent.replaceChild(newTreeChildrenNode, treeChildren);
    }
  }

  AppendnewRowAndSetFocus();

  // focus on first name
  var listName = document.getElementById('ListName');
  if ( listName )
    listName.focus();
  moveToAlertPosition();
}

function AppendnewRowAndSetFocus()
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
    // loose the content when the field is not visible. See bug 37435
      input[0].setAttribute("value", inputValue);
      input[0].value = inputValue;
      input[0].setAttribute("id", "address#" + top.MAX_RECIPIENTS);
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

function awClickEmptySpace(targ, setFocus)
{
  if (targ.localName != 'treechildren')
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
        inputElementType = document.getElementById("address#1").localName;
    return inputElementType;
}

function awAppendNewRow(setFocus)
{
  var body = document.getElementById('addressList');
  var treeitem1 = awGetTreeItem(1);

  if ( body && treeitem1 )
  {
    var newNode = awCopyNode(treeitem1, body, 0);
    top.MAX_RECIPIENTS++;

        var input = newNode.getElementsByTagName(awInputElementName());
        if ( input && input.length == 1 )
        {
          input[0].setAttribute("value", "");
          input[0].setAttribute("id", "address#" + top.MAX_RECIPIENTS);
      }
    // focus on new input widget
    if (setFocus && input )
      awSetFocus(top.MAX_RECIPIENTS, input[0]);
  }
}


// functions for accessing the elements in the addressing widget

function awGetInputElement(row)
{
    return document.getElementById("address#" + row);
}

function awGetTreeRow(row)
{
  var body = document.getElementById('addressList');

  if ( body && row > 0)
  {
    var treerows = body.getElementsByTagName('treerow');
    if ( treerows && treerows.length >= row )
      return treerows[row-1];
  }
  return 0;
}

function awGetTreeItem(row)
{
  var body = document.getElementById('addressList');

  if ( body && row > 0)
  {
    var treeitems = body.getElementsByTagName('treeitem');
    if ( treeitems && treeitems.length >= row )
      return treeitems[row-1];
  }
  return 0;
}

function awGetRowByInputElement(inputElement)
{
  if ( inputElement )
  {
    var treerow;
    var inputElementTreerow = inputElement.parentNode.parentNode;

    if ( inputElementTreerow )
    {
      for ( var row = 1;  (treerow = awGetTreeRow(row)); row++ )
      {
        if ( treerow == inputElementTreerow )
        {
          return row;
        }
      }
    }
  }
  return 0;
}


// Copy Node - copy this node and insert ahead of the (before) node.  Append to end if before=0
function awCopyNode(node, parentNode, beforeNode)
{
  var newNode = node.cloneNode(true);

  if ( beforeNode )
    parentNode.insertBefore(newNode, beforeNode);
  else
    parentNode.appendChild(newNode);

    return newNode;
}

// remove row

function awRemoveRow(row)
{
  var body = document.getElementById('addressList');

  awRemoveNodeAndChildren(body, awGetTreeItem(row));

  top.MAX_RECIPIENTS--;
}

function awRemoveNodeAndChildren(parent, nodeToRemove)
{
  // children of nodes
  var childNode;

  while ( nodeToRemove.childNodes && nodeToRemove.childNodes.length )
  {
    childNode = nodeToRemove.childNodes[0];

    awRemoveNodeAndChildren(nodeToRemove, childNode);
  }

  parent.removeChild(nodeToRemove);

}

function awSetFocus(row, inputElement)
{
  top.awRow = row;
  top.awInputElement = inputElement;
  top.awFocusRetry = 0;
  setTimeout("_awSetFocus();", 0);
}

function _awSetFocus()
{
  var tree = document.getElementById('addressListTree');
  try
  {
    var theNewRow = awGetTreeRow(top.awRow);
    //temporary patch for bug 26344
//    awFinishCopyNode(theNewRow);

    tree.ensureElementIsVisible(theNewRow);
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


//temporary patch for bug 26344 & 26528
function awFinishCopyNode(node)
{
    msgCompose.ResetNodeEventHandlers(node);
    return;
}


function awFinishCopyNodes()
{
  var treeChildren = document.getElementById('addressList');
    awFinishCopyNode(treeChildren);
}


function awTabFromRecipient(element, event)
{
  //If we are le last element in the tree, we don't want to create a new row.
  if (element == awGetInputElement(top.MAX_RECIPIENTS))
    top.doNotCreateANewRow = true;
}

function awGetNumberOfRecipients()
{
    return top.MAX_RECIPIENTS;
}

function DragOverTree(event)
{
  var validFlavor = false;
  var dragSession = null;
  var retVal = true;

  var dragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
  if (dragService)
    dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
  if (!dragService) return(false);

  dragSession = dragService.getCurrentSession();
  if (!dragSession) return(false);

  if (dragSession.isDataFlavorSupported("text/nsabcard")) validFlavor = true;
  //XXX other flavors here...

  // touch the attribute on the rowgroup to trigger the repaint with the drop feedback.
  if (validFlavor)
  {
    //XXX this is really slow and likes to refresh N times per second.
    var rowGroup = event.target.parentNode.parentNode;
    rowGroup.setAttribute ( "dd-triggerrepaint", 0 );
    dragSession.canDrop = true;
    // necessary??
    retVal = false; // do not propagate message
  }
  return(retVal);
}

function DropOnAddressListTree(event)
{
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
  if (rdf)
    rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
  if (!rdf) return(false);

  var dragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
  if (dragService)
    dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
  if (!dragService) return(false);

  var dragSession = dragService.getCurrentSession();
  if ( !dragSession ) return(false);

  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
  if ( !trans ) return(false);
  trans.addDataFlavor("text/nsabcard");

  for ( var i = 0; i < dragSession.numDropItems; ++i )
  {
    dragSession.getData ( trans, i );
    var dataObj = new Object();
    var bestFlavor = new Object();
    var len = new Object();
    trans.getAnyTransferData ( bestFlavor, dataObj, len );
    if ( dataObj )  dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
    if ( !dataObj ) continue;

    // pull the URL out of the data object
    var sourceID = dataObj.data.substring(0, len.value);
    if (!sourceID)  continue;

    var cardResource = rdf.GetResource(sourceID);
    var card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);

    if (card.isMailList)
      DropListAddress(card.name);
    else
    {
      var address;
      if (card.name.length)
        address = card.name + " <" + card.primaryEmail + ">";
      else
        address = card.primaryEmail;
      DropListAddress(address);
    }

  }

  return(false);
}

function DropListAddress(address)
{
    awClickEmptySpace(true);    //that will automatically set the focus on a new available row, and make sure is visible
    if (top.MAX_RECIPIENTS == 0)
    top.MAX_RECIPIENTS = 1;
  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
    lastInput.value = address;
    awAppendNewRow(true);
}
