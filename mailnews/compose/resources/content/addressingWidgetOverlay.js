/*
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

top.MAX_RECIPIENTS = 0;

var inputElementType = "";
var selectElementType = "";
var selectElementIndexTable = null;

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
	    selectElem = document.getElementById("msgRecipientType#1");
        for (var i = 0; i < selectElem.childNodes[0].childNodes.length; i ++)
    {
            aData = selectElem.childNodes[0].childNodes[i].getAttribute("data");
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

	    while ((inputField = awGetInputElement(i)))
	    {
	    	fieldValue = inputField.value;
	    	if (fieldValue == null)
	    	  fieldValue = inputField.getAttribute("value");

	    	if (fieldValue != "")
	    	{
			    switch (awGetPopupElement(i).selectedItem.getAttribute("data"))
	    		{
	    			case "addr_to"			: addrTo += to_Sep + fieldValue; to_Sep = ",";					break;
	    			case "addr_cc"			: addrCc += cc_Sep + fieldValue; cc_Sep = ",";					break;
	    			case "addr_bcc"			: addrBcc += bcc_Sep + fieldValue; bcc_Sep = ",";				break;
	    			case "addr_reply"		: addrReply += reply_Sep + fieldValue; reply_Sep = ",";			break;
	    			case "addr_newsgroups"	: addrNg += ng_Sep + fieldValue; ng_Sep = ",";					break;
	    			case "addr_followup"	: addrFollow += follow_Sep + fieldValue; follow_Sep = ",";		break;
					case "addr_other"		: addrOther += other_header + ": " + fieldValue + "\n"; 		break;
	    		}
	    	}
	    	i ++;
	    }
    	msgCompFields.SetTo(addrTo);
    	msgCompFields.SetCc(addrCc);
    	msgCompFields.SetBcc(addrBcc);
    	msgCompFields.SetReplyTo(addrReply);
    	msgCompFields.SetNewsgroups(addrNg);
    	msgCompFields.SetFollowupTo(addrFollow);
        msgCompFields.SetOtherRandomHeaders(addrOther);
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

		awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCompFields.GetReplyTo(), false), "addr_reply", newTreeChildrenNode, templateNode);
		awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCompFields.GetTo(), false), "addr_to", newTreeChildrenNode, templateNode);
		awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCompFields.GetCc(), false), "addr_cc", newTreeChildrenNode, templateNode);
		awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCompFields.GetBcc(), false), "addr_bcc", newTreeChildrenNode, templateNode);
		awSetInputAndPopup(msgCompFields.GetOtherRandomHeaders(), "addr_other", newTreeChildrenNode, templateNode);
		awSetInputAndPopup(msgCompFields.GetNewsgroups(), "addr_newsgroups", newTreeChildrenNode, templateNode);
		awSetInputAndPopup(msgCompFields.GetFollowupTo(), "addr_followup", newTreeChildrenNode, templateNode);
		
    if (top.MAX_RECIPIENTS == 0)
		{
		  top.MAX_RECIPIENTS = 1;
			awSetAutoComplete(top.MAX_RECIPIENTS);
		}
		else
		{
		    //If it's a new message, we need to add an extrat empty recipient.
		    var msgComposeType = Components.interfaces.nsIMsgCompType;
		    if (msgType == msgComposeType.New)
		        _awSetInputAndPopup("", "addr_to", newTreeChildrenNode, templateNode);
	        var parent = treeChildren.parentNode;
	        parent.replaceChild(newTreeChildrenNode, treeChildren);
            setTimeout("awFinishCopyNodes();", 0);
        }
	}
}

function awSetInputAndPopupValue(inputElem, inputValue, popupElem, popupValue, rowNumber)
{
	// remove leading spaces
	while (inputValue[0] == " " )
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
      if (popup.selectedItem.getAttribute("data") == recipientType)
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
}

function awClickEmptySpace(targ, setFocus)
{
  if (targ.localName != 'treechildren')
    return;

	dump("awClickEmptySpace\n");
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

function awDeleteHit(inputElement)
{
  var row = awGetRowByInputElement(inputElement);
  var nextRow = awGetInputElement(row+1);
  var index = 1;
  if (!nextRow) {
    nextRow = awGetInputElement(row-1);
    index = -1;
  }
  if (nextRow) {
    awSetFocus(row+index, nextRow)
    if (row)
      awRemoveRow(row);
  }
  else
    inputElement.value = "";
}

function awInputChanged(inputElement)
{
	dump("awInputChanged\n");
//	AutoCompleteAddress(inputElement);

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
	    var lastRecipientType = awGetPopupElement(top.MAX_RECIPIENTS).selectedItem.getAttribute("data");

		newNode = awCopyNode(treeitem1, body, 0);
		top.MAX_RECIPIENTS++;

        var input = newNode.getElementsByTagName(awInputElementName());
        if ( input && input.length == 1 )
        {
   			  input[0].setAttribute("value", "");
    	    input[0].setAttribute("id", "msgRecipient#" + top.MAX_RECIPIENTS);
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
		theNewRow = awGetTreeRow(top.awRow);
		//temporary patch for bug 26344
		awFinishCopyNode(theNewRow);

		tree.ensureElementIsVisible(theNewRow);
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

	var dragService = Components.classes["component://netscape/widget/dragservice"].getService();
	if (dragService) 
		dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if (!dragService)	return(false);

	dragSession = dragService.getCurrentSession();
	if (!dragSession)	return(false);

	if (dragSession.isDataFlavorSupported("text/nsabcard"))	validFlavor = true;
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
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	if (rdf)   
		rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
	if (!rdf) return(false);

	var dragService = Components.classes["component://netscape/widget/dragservice"].getService();
	if (dragService) 
		dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if (!dragService)	return(false);
	
	var dragSession = dragService.getCurrentSession();
	if ( !dragSession )	return(false);

	var trans = Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
	if ( !trans ) return(false);
	trans.addDataFlavor("text/nsabcard");

	for ( var i = 0; i < dragSession.numDropItems; ++i )
	{
		dragSession.getData ( trans, i );
		dataObj = new Object();
		bestFlavor = new Object();
		len = new Object();
		trans.getAnyTransferData ( bestFlavor, dataObj, len );
		if ( dataObj )	dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
		if ( !dataObj )	continue;

		// pull the URL out of the data object
		var sourceID = dataObj.data.substring(0, len.value);
		if (!sourceID)	continue;

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
	if (selectElem.data != 'addr_newsgroups' && selectElem.data != 'addr_followup')
		inputElem.disableAutocomplete = false;
	else
    inputElem.disableAutocomplete = true;
}

function awSetAutoComplete(rowNumber)
{
    inputElem = awGetInputElement(rowNumber);
    selectElem = awGetPopupElement(rowNumber);
    _awSetAutoComplete(selectElem, inputElem)
}

function awRecipientKeyPress(event, element)
{
  switch(event.keyCode) {
  case 13:
    awReturnHit(element);
    break;
  case 9:
    awTabFromRecipient(element, event);
    break;
  case 46:
  case 8:
    if (!element.value && !this.lastVal)
      awDeleteHit(element);
    break;
  }
  this.lastVal = element.value;
}

function awKeyPress(event, treeElement)
{
  switch(event.keyCode) {
  case 46:
  case 8:
    var selItems = treeElement.selectedItems;
    var kids = document.getElementById("addressWidgetBody");
    for (var i = 0; i < selItems.length; i++) {
      // must not delete the last item
      if (kids.childNodes.length > 1)
        kids.removeChild(selItems[i]);
      else
        selItems[i].firstChild.lastChild.childNodes[1].value = "";
      top.MAX_RECIPIENTS--;
    }
    break;   
  }
}

