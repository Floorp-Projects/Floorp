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

top.MAX_RECIPIENTS = 0;

var inputElementType = "";
var selectElementType = "";
var selectElementIndexTable = null;
var gPromptService = null;

var test_addresses_sequence = false;
if (prefs)
  try {
    test_addresses_sequence = prefs.getBoolPref("mail.debug.test_addresses_sequence");
  }
  catch (ex) {}

function awInputElementName()
{
    if (inputElementType == "")
        inputElementType = document.getElementById("msgRecipient#1").localName;
    return inputElementType;
}

function awSelectElementName()
{
    if (selectElementType == "")
        selectElementType = document.getElementById("msgRecipientType#1").localName;
    return selectElementType;
}

function awGetSelectItemIndex(itemData)
{
    if (selectElementIndexTable == null)
    {
      selectElementIndexTable = new Object();
      var selectElem = document.getElementById("msgRecipientType#1");
        for (var i = 0; i < selectElem.childNodes[0].childNodes.length; i ++)
    {
            var aData = selectElem.childNodes[0].childNodes[i].getAttribute("value");
            selectElementIndexTable[aData] = i;
        }
    }
    return selectElementIndexTable[itemData];
}

function Recipients2CompFields(msgCompFields)
{
  if (msgCompFields)
  {
    var i = 1;
    var addrTo = "";
    var addrCc = "";
    var addrBcc = "";
    var addrReply = "";
    var addrNg = "";
    var addrFollow = "";
    var addrOther = "";
    var to_Sep = "";
    var cc_Sep = "";
    var bcc_Sep = "";
    var reply_Sep = "";
    var ng_Sep = "";
    var follow_Sep = "";

    var inputField;
      while ((inputField = awGetInputElement(i)))
      {
        var fieldValue = inputField.value;

        if (fieldValue == null)
          fieldValue = inputField.getAttribute("value");

        if (fieldValue != "")
        {
          switch (awGetPopupElement(i).selectedItem.getAttribute("value"))
          {
            case "addr_to"      : addrTo += to_Sep + fieldValue; to_Sep = ",";          break;
            case "addr_cc"      : addrCc += cc_Sep + fieldValue; cc_Sep = ",";          break;
            case "addr_bcc"     : addrBcc += bcc_Sep + fieldValue; bcc_Sep = ",";       break;
            case "addr_reply"   : addrReply += reply_Sep + fieldValue; reply_Sep = ",";     break;
            case "addr_newsgroups"  : addrNg += ng_Sep + fieldValue; ng_Sep = ",";          break;
            case "addr_followup"  : addrFollow += follow_Sep + fieldValue; follow_Sep = ",";    break;
          case "addr_other"   : addrOther += other_header + ": " + fieldValue + "\n";     break;
          }
        }
        i ++;
      }

      msgCompFields.to = addrTo;
      msgCompFields.cc = addrCc;
      msgCompFields.bcc = addrBcc;
      msgCompFields.replyTo = addrReply;
      msgCompFields.newsgroups = addrNg;
      msgCompFields.followupTo = addrFollow;
      msgCompFields.otherRandomHeaders = addrOther;
  }
  else
    dump("Message Compose Error: msgCompFields is null (ExtractRecipients)");
}

function CompFields2Recipients(msgCompFields, msgType)
{
  if (msgCompFields)
  {
      var treeChildren = document.getElementById('addressWidgetBody');
      var newTreeChildrenNode = treeChildren.cloneNode(false);
      var templateNode = treeChildren.firstChild;

    top.MAX_RECIPIENTS = 0;
    var msgReplyTo = msgCompFields.replyTo;
    var msgTo = msgCompFields.to;
    var msgCC = msgCompFields.cc;
    var msgBCC = msgCompFields.bcc;
    var msgRandomHeaders = msgCompFields.otherRandomHeaders;
    var msgNewsgroups = msgCompFields.newsgroups;
    var msgFollowupTo = msgCompFields.followupTo;
    if(msgReplyTo)
      awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgReplyTo, false), "addr_reply", newTreeChildrenNode, templateNode);
    if(msgTo)
      awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgTo, false), "addr_to", newTreeChildrenNode, templateNode);
    if(msgCC)
      awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCC, false), "addr_cc", newTreeChildrenNode, templateNode);
    if(msgBCC)
      awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgBCC, false), "addr_bcc", newTreeChildrenNode, templateNode);
    if(msgRandomHeaders)
      awSetInputAndPopup(msgRandomHeaders, "addr_other", newTreeChildrenNode, templateNode);
    if(msgNewsgroups)
      awSetInputAndPopup(msgNewsgroups, "addr_newsgroups", newTreeChildrenNode, templateNode);
    if(msgFollowupTo)
      awSetInputAndPopup(msgFollowupTo, "addr_followup", newTreeChildrenNode, templateNode);

    //If it's a new message, we need to add an extrat empty recipient.
    if (!msgTo && !msgNewsgroups)
      _awSetInputAndPopup("", "addr_to", newTreeChildrenNode, templateNode);
      // dump("replacing child in comp fields 2 recips \n");
      var parent = treeChildren.parentNode;
      parent.replaceChild(newTreeChildrenNode, treeChildren);
      awFitDummyRows();
        setTimeout("awFinishCopyNodes();", 0);
  }
}

function awSetInputAndPopupValue(inputElem, inputValue, popupElem, popupValue, rowNumber)
{
  // remove leading spaces
  while (inputValue && inputValue[0] == " " )
    inputValue = inputValue.substring(1, inputValue.length);

  inputElem.setAttribute("value", inputValue);
  inputElem.value = inputValue;

  popupElem.selectedItem = popupElem.childNodes[0].childNodes[awGetSelectItemIndex(popupValue)];

  if (rowNumber >= 0)
  {
    inputElem.setAttribute("id", "msgRecipient#" + rowNumber);
    popupElem.setAttribute("id", "msgRecipientType#" + rowNumber);
  }

  _awSetAutoComplete(popupElem, inputElem);
}

function _awSetInputAndPopup(inputValue, popupValue, parentNode, templateNode)
{
    top.MAX_RECIPIENTS++;

    var newNode = templateNode.cloneNode(true);
    parentNode.appendChild(newNode); // we need to insert the new node before we set the value of the select element!

    var input = newNode.getElementsByTagName(awInputElementName());
    var select = newNode.getElementsByTagName(awSelectElementName());

    if (input && input.length == 1 && select && select.length == 1)
      awSetInputAndPopupValue(input[0], inputValue, select[0], popupValue, top.MAX_RECIPIENTS)
}

function awSetInputAndPopup(inputValue, popupValue, parentNode, templateNode)
{
  if ( inputValue && popupValue )
  {
    var addressArray = inputValue.split(",");

    for ( var index = 0; index < addressArray.length; index++ )
        _awSetInputAndPopup(addressArray[index], popupValue, parentNode, templateNode);
  }
}

function awSetInputAndPopupFromArray(inputArray, popupValue, parentNode, templateNode)
{
  if ( inputArray && popupValue )
  {
    for ( var index = 0; index < inputArray.count; index++ )
        _awSetInputAndPopup(inputArray.StringAt(index), popupValue, parentNode, templateNode);
  }
}

function awRemoveRecipients(msgCompFields, recipientType, recipientsList)
{
  if (!msgCompFields)
    return;

  var recipientArray = msgCompFields.SplitRecipients(recipientsList, false);
  if (! recipientArray)
    return;

  for ( var index = 0; index < recipientArray.count; index++ )
    for (var row = 1; row <= top.MAX_RECIPIENTS; row ++)
    {
      var popup = awGetPopupElement(row);
      if (popup.selectedItem.getAttribute("value") == recipientType)
      {
        var input = awGetInputElement(row);
        if (input.value == recipientArray.StringAt(index))
        {
          awSetInputAndPopupValue(input, "", popup, "addr_to", -1);
          break;
        }
      }
    }
}

function awAddRecipients(msgCompFields, recipientType, recipientsList)
{
  if (!msgCompFields)
    return;

  var recipientArray = msgCompFields.SplitRecipients(recipientsList, false);
  if (! recipientArray)
    return;

  for ( var index = 0; index < recipientArray.count; index++ )
  {
    for (var row = 1; row <= top.MAX_RECIPIENTS; row ++)
    {
      if (awGetInputElement(row).value == "")
        break;
    }
    if (row > top.MAX_RECIPIENTS)
      awAppendNewRow(false);

    awSetInputAndPopupValue(awGetInputElement(row), recipientArray.StringAt(index), awGetPopupElement(row), recipientType, row);

    /* be sure we still have an empty row left at the end */
    if (row == top.MAX_RECIPIENTS)
    {
      awAppendNewRow(true);
      awSetInputAndPopupValue(awGetInputElement(top.MAX_RECIPIENTS), "", awGetPopupElement(top.MAX_RECIPIENTS), "addr_to", top.MAX_RECIPIENTS);
    }
  }
}

function awTestRowSequence()
{
  /*
    This function is for debug and testing purpose only, normal user should not run it!

    Everytime we insert or delete a row, we must be sure we didn't break the ID sequence of
    the addressing widget rows. This function will run a quick test to see if the sequence still ok

    You need to define the pref mail.debug.test_addresses_sequence to true in order to activate it
  */

  if (! test_addresses_sequence)
    return true;

  /* debug code to verify the sequence still good */
  var body = document.getElementById('addressWidgetBody');
  var treeitems = body.getElementsByTagName('treeitem');
  if (treeitems.length == top.MAX_RECIPIENTS )
  {
    for (var i = 1; i <= treeitems.length; i ++)
    {
      var item = treeitems[i - 1];
      var inputID = item.getElementsByTagName(awInputElementName())[0].getAttribute("id").split("#")[1];
      var popupID = item.getElementsByTagName(awSelectElementName())[0].getAttribute("id").split("#")[1];
      if (inputID != i || popupID != i)
      {
        dump("#ERROR: sequence broken at row " + i + ", inputID=" + inputID + ", popupID=" + popupID + "\n");
        break;
      }
    }
    dump("---SEQUENCE OK---\n");
    return true;
  }
  else
    dump("#ERROR: treeitems.length(" + treeitems.length + ") != top.MAX_RECIPIENTS(" + top.MAX_RECIPIENTS + ")\n");

  return false;
}

function awCleanupRows()
{
  var maxRecipients = top.MAX_RECIPIENTS;
  var rowID = 1;

  for (var row = 1; row <= maxRecipients; row ++)
  {
    var inputElem = awGetInputElement(row);
    if (inputElem.value == "" && row < maxRecipients)
      awRemoveRow(row);
    else
    {
      inputElem.setAttribute("id", "msgRecipient#" + rowID);
      awGetPopupElement(row).setAttribute("id", "msgRecipientType#" + rowID);
      rowID ++;
    }
  }

  awTestRowSequence();
}

function awDeleteRow(rowToDelete)
{
  /* When we delete a row, we must reset the id of others row in order to not break the sequence */
  var maxRecipients = top.MAX_RECIPIENTS;
  var rowID = rowToDelete;

  awRemoveRow(rowToDelete);

  for (var row = rowToDelete + 1; row <= maxRecipients; row ++)
  {
    awGetInputElement(row).setAttribute("id", "msgRecipient#" + rowID);
    awGetPopupElement(row).setAttribute("id", "msgRecipientType#" + rowID);
    rowID ++;
  }

  awTestRowSequence();
}

function awClickEmptySpace(targ, setFocus)
{
  if (targ.localName != 'treechildren')
    return;

  // dump("awClickEmptySpace\n");
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
  var nextInput = awGetInputElement(row+1);

  if ( !nextInput )
  {
    if ( inputElement.value )
      awAppendNewRow(true);
    else // No adress entered, switch to Subject field
    {
      var subjectField = document.getElementById( 'msgSubject' );
      subjectField.select();
      subjectField.focus();
    }
  }
  else
  {
    nextInput.select();
    awSetFocus(row+1, nextInput);
  }
}

function awDeleteHit(inputElement)
{
  var row = awGetRowByInputElement(inputElement);

  /* 1. don't delete the row if it's the last one remaining, just reset it! */
  if (top.MAX_RECIPIENTS <= 1)
  {
    inputElement.value = "";
    return;
  }

  /* 2. Set the focus to the previous field if possible */
  if (row > 1)
    awSetFocus(row - 1, awGetInputElement(row - 1))
  else
    awSetFocus(1, awGetInputElement(2))   /* We have to cheat a little bit because the focus will */
                                          /* be set asynchronusly after we delete the current row, */
                                          /* therefore the row number still the same! */

  /* 3. Delete the row */
  awDeleteRow(row);
}

function awInputChanged(inputElement)
{
  dump("awInputChanged\n");
//  AutoCompleteAddress(inputElement);

  //Do we need to add a new row?
  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
  if ( lastInput && lastInput.value && !top.doNotCreateANewRow)
    awAppendNewRow(false);
  top.doNotCreateANewRow = false;
}

function awAppendNewRow(setFocus)
{
  var body = document.getElementById('addressWidgetBody');
  var treeitem1 = awGetTreeItem(1);

  if ( body && treeitem1 )
  {
      var lastRecipientType = awGetPopupElement(top.MAX_RECIPIENTS).selectedItem.getAttribute("value");

    var nextDummy = awGetNextDummyRow();
    if (nextDummy)  {
      body.removeChild(nextDummy);
      nextDummy = awGetNextDummyRow();
    }
    var newNode = awCopyNode(treeitem1, body, nextDummy);

    top.MAX_RECIPIENTS++;

        var input = newNode.getElementsByTagName(awInputElementName());
        if ( input && input.length == 1 )
        {
          input[0].setAttribute("value", "");
          input[0].setAttribute("id", "msgRecipient#" + top.MAX_RECIPIENTS);

          //this copies the autocomplete sessions list from recipient#1 
          input[0].syncSessions(document.getElementById('msgRecipient#1'));

	  // also clone the showCommentColumn setting
	  //
	  input[0].showCommentColumn = 
	      document.getElementById("msgRecipient#1").showCommentColumn;

          // We always clone the first row.  The problem is that the first row
          // could be focused.  When we clone that row, we end up with a cloned
          // XUL textbox that has a focused attribute set.  Therefore we think
          // we're focused and don't properly refocus.  The best solution to this
          // would be to clone a template row that didn't really have any presentation,
          // rather than using the real visible first row of the tree.
          //
          // For now we'll just put in a hack that ensures the focused attribute
          // is never copied when the node is cloned.
          if (input[0].getAttribute('focused') != '')
            input[0].removeAttribute('focused');
        }
        var select = newNode.getElementsByTagName(awSelectElementName());
        if ( select && select.length == 1 )
        {
            select[0].selectedItem = select[0].childNodes[0].childNodes[awGetSelectItemIndex(lastRecipientType)];
          select[0].setAttribute("id", "msgRecipientType#" + top.MAX_RECIPIENTS);
          if (input)
            _awSetAutoComplete(select[0], input[0]);
      }

    // focus on new input widget
    if (setFocus && input[0] )
      awSetFocus(top.MAX_RECIPIENTS, input[0]);
  }
}


// functions for accessing the elements in the addressing widget

function awGetPopupElement(row)
{
    return document.getElementById("msgRecipientType#" + row);
}

function awGetInputElement(row)
{
    return document.getElementById("msgRecipient#" + row);
}

function awGetTreeRow(row)
{
  var body = document.getElementById('addressWidgetBody');

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
  var body = document.getElementById('addressWidgetBody');

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
          return row;
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
  var body = document.getElementById('addressWidgetBody');

  awRemoveNodeAndChildren(body, awGetTreeItem(row));
  awFitDummyRows();

  top.MAX_RECIPIENTS --;
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
  var tree = document.getElementById('addressingWidgetTree');
  try
  {
    var theNewRow = awGetTreeRow(top.awRow);
    //temporary patch for bug 26344
    awFinishCopyNode(theNewRow);

    //Warning: firstVisibleRow is zero base but top.awRow is one base!
    var firstVisibleRow = tree.getIndexOfFirstVisibleRow();
    var numOfVisibleRows = tree.getNumberOfVisibleRows();

    //Do we need to scroll in order to see the selected row?
    if (top.awRow <= firstVisibleRow)
      tree.scrollToIndex(top.awRow - 1);
    else
      if (top.awRow - 1 >= (firstVisibleRow + numOfVisibleRows))
        tree.scrollToIndex(top.awRow - numOfVisibleRows);

    top.awInputElement.focus();
  }
  catch(ex)
  {
    top.awFocusRetry ++;
    if (top.awFocusRetry < 3)
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
  var treeChildren = document.getElementById('addressWidgetBody');
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

function DropOnAddressingWidgetTree(event)
{
  dump("DropOnTree\n");
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
    dataObj = new Object();
    bestFlavor = new Object();
    len = new Object();
    trans.getAnyTransferData ( bestFlavor, dataObj, len );
    if ( dataObj )  dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
    if ( !dataObj ) continue;

    // pull the URL out of the data object
    var sourceID = dataObj.data.substring(0, len.value);
    if (!sourceID)  continue;

    var cardResource = rdf.GetResource(sourceID);
    var card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
    var address = "\"" + card.name + "\" <" + card.primaryEmail + ">";
    dump("    Address #" + i + " = " + address + "\n");

    DropRecipient(address);

  }

  return(false);
}

function DropRecipient(recipient)
{
    awClickEmptySpace(true);    //that will automatically set the focus on a new available row, and make sure is visible
    var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
    lastInput.value = recipient;
    awAppendNewRow(true);
}

function _awSetAutoComplete(selectElem, inputElem)
{
  if (selectElem.value != 'addr_newsgroups' && selectElem.value != 'addr_followup')
    inputElem.disableAutocomplete = false;
  else
    inputElem.disableAutocomplete = true;
}

function awSetAutoComplete(rowNumber)
{
    var inputElem = awGetInputElement(rowNumber);
    var selectElem = awGetPopupElement(rowNumber);
    _awSetAutoComplete(selectElem, inputElem)
}

function awRecipientTextCommand(userAction, element)
{
  if (userAction == "typing" || userAction == "scrolling")
    awReturnHit(element);
}

// Called when an autocomplete session item is selected and the status of
// the session it was selected from is nsIAutoCompleteStatus::failureItems.
//
// As of this writing, the only way that can happen is when an LDAP 
// autocomplete session returns an error to be displayed to the user.
//
// There are hardcoded messages in here, but these are just fallbacks for
// when string bundles have already failed us.
//
function awRecipientErrorCommand(errItem, element)
{
    // remove the angle brackets from the general error message to construct 
    // the title for the alert.  someday we'll pass this info using a real
    // exception object, and then this code can go away.
    //
    var generalErrString;
    if (errItem.value != "") {
	generalErrString = errItem.value.slice(1, errItem.value.length-1);
    } else {
	generalErrString = "Unknown LDAP server problem encountered";
    }	

    // try and get the string of the specific error to contruct the complete
    // err msg, otherwise fall back to something generic.  This message is
    // handed to us as an nsISupportsWString in the param slot of the 
    // autocomplete error item, by agreement documented in 
    // nsILDAPAutoCompFormatter.idl
    //
    var specificErrString = "";
    try {
	var specificError = errItem.param.QueryInterface(
	    Components.interfaces.nsISupportsWString);
	specificErrString = specificError.data;
    } catch (ex) {
    }
    if (specificErrString == "") {
	specificErrString = "Internal error";
    }

    try {
	if (!gPromptService) {
	    gPromptService = Components.classes[
		"@mozilla.org/embedcomp/prompt-service;1"].getService().
		QueryInterface(Components.interfaces.nsIPromptService);
	}
    } catch (ex) {
    }

    if (gPromptService) {
	gPromptService.alert(window, generalErrString, specificErrString);
    } else {
	window.alert(generalErrString + ": " + specificErrString);
    }
}

function awRecipientKeyPress(event, element)
{
  switch(event.keyCode) {
  case 9:
    awTabFromRecipient(element, event);
    break;
  }
}

function awRecipientKeyDown(event, element)
{
  switch(event.keyCode) {
  case 46:
  case 8:
    /* do not query directly the value of the text field else the autocomplete widget could potentially
       alter it value while doing some internal cleanup, instead, query the value through the first child
    */
    if (!element.value)
      awDeleteHit(element);
    event.preventBubble();  //We need to stop the event else the tree will receive it and the function
                            //awKeyDown will be executed!
    break;
  }
}

function awKeyDown(event, treeElement)
{
  switch(event.keyCode) {
  case 46:
  case 8:
    /* Warning, the treeElement.selectedItems will change everytime we delete a row */
    var selItems = treeElement.selectedItems;
    var length = treeElement.selectedItems.length;
    for (var i = 1; i <= length; i++) {
      var inputs = treeElement.selectedItems[0].getElementsByTagName(awInputElementName());
      if (inputs && inputs.length == 1)
        awDeleteHit(inputs[0]);
    }
    break;
  }
}

/* ::::::::::: addressing widget dummy rows ::::::::::::::::: */

var gAWContentHeight = 0;
var gAWRowHeight = 0;

function awFitDummyRows()
{
  awCalcContentHeight();
  awCreateOrRemoveDummyRows();
}

function awCreateOrRemoveDummyRows()
{
  var body = document.getElementById("addressWidgetBody");
  var bodyHeight = body.boxObject.height;

  // remove rows to remove scrollbar
  var kids = body.childNodes;
  for (var i = kids.length-1; gAWContentHeight > bodyHeight && i >= 0; --i) {
    if (kids[i].hasAttribute("_isDummyRow")) {
      gAWContentHeight -= gAWRowHeight;
      body.removeChild(kids[i]);
    }
  }

  // add rows to fill space
  if (gAWRowHeight) {
    while (gAWContentHeight+gAWRowHeight < bodyHeight) {
      awCreateDummyItem(body);
      gAWContentHeight += gAWRowHeight;
    }
  }
}

function awCalcContentHeight()
{
  var body = document.getElementById("addressWidgetBody");
  var kids = body.getElementsByTagName("treerow");

  gAWContentHeight = 0;
  if (kids.length > 0) {
    // all rows are forced to a uniform height in xul trees, so
    // find the first tree row with a boxObject and use it as precedent
    var i = 0;
    do {
      gAWRowHeight = kids[i].boxObject.height;
      ++i;
    } while (i < kids.length && !gAWRowHeight);
    gAWContentHeight = gAWRowHeight*kids.length;
  }
}

function awCreateDummyItem(aParent)
{
  var titem = document.createElement("treeitem");
  titem.setAttribute("_isDummyRow", "true");

  var trow = document.createElement("treerow");
  trow.setAttribute("class", "dummy-row");
  trow.setAttribute("onclick", "awDummyRow_onclick()");
  titem.appendChild(trow);

  awCreateDummyCell(trow);
  awCreateDummyCell(trow);

  if (aParent)
    aParent.appendChild(titem);

  return titem;
}

function awCreateDummyCell(aParent)
{
  var cell = document.createElement("treecell");
  cell.setAttribute("class", "treecell-addressingWidget dummy-row-cell");
  if (aParent)
    aParent.appendChild(cell);

  return cell;
}

function awDummyRow_onclick() {
  // pass click event back to handler
  awClickEmptySpace(document.getElementById("addressWidgetBody"), true);
}

function awGetNextDummyRow()
{
  // gets the next row from the top down
  var body = document.getElementById("addressWidgetBody");
  var kids = body.childNodes;
  for (var i = 0; i < kids.length; ++i) {
    if (kids[i].hasAttribute("_isDummyRow"))
      return kids[i];
  }
  return null;
}

function awSizerListen()
{
  // when splitter is clicked, fill in necessary dummy rows each time the mouse is moved
  awCalcContentHeight(); // precalculate
  document.addEventListener("mousemove", awSizerMouseMove, true);
  document.addEventListener("mouseup", awSizerMouseUp, false);
}

function awSizerMouseMove()
{
  awCreateOrRemoveDummyRows();
}

function awSizerMouseUp()
{
  document.removeEventListener("mousemove", awSizerMouseUp, false);
  document.removeEventListener("mouseup", awSizerMouseUp, false);
}

