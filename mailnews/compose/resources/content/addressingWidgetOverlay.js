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
	    	if (fieldValue != "")
	    	{
	    		switch (awGetPopupElement(i).value)
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
//	    var templateNode = treeChildren.firstChild();  // doesn't work!
	    var templateNode = awGetTreeItem(1);	    
		
		awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCompFields.GetReplyTo(), false), "addr_reply", newTreeChildrenNode, templateNode);
		awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCompFields.GetTo(), false), "addr_to", newTreeChildrenNode, templateNode);
		awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCompFields.GetCc(), false), "addr_cc", newTreeChildrenNode, templateNode);
		awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCompFields.GetBcc(), false), "addr_bcc", newTreeChildrenNode, templateNode);
		awSetInputAndPopup(msgCompFields.GetOtherRandomHeaders(), "addr_other", newTreeChildrenNode, templateNode);
		awSetInputAndPopup(msgCompFields.GetNewsgroups(), "addr_newsgroups", newTreeChildrenNode, templateNode);
		awSetInputAndPopup(msgCompFields.GetFollowupTo(), "addr_followup", newTreeChildrenNode, templateNode);
		
        if (top.MAX_RECIPIENTS == 0)
		    top.MAX_RECIPIENTS = 1;
		else
		{
		    //If it's a new message, we need to add an extrat empty recipient.
		    var msgComposeType = Components.interfaces.nsIMsgCompType;
		    if (msgType == msgComposeType.New)
		        _awSetInputAndPopup("", "addr_to", newTreeChildrenNode, templateNode);
//	        var parent = treeChildren.parentNode();     // doesn't work!
	        var parent = document.getElementById('addressingWidgetTree')
	        parent.replaceChild(newTreeChildrenNode, treeChildren);
            setTimeout("awFinishCopyNodes();", 0);
        }
	}
}

function _awSetInputAndPopup(inputValue, popupValue, parentNode, templateNode)
{
	// remove leading spaces
	while (inputValue[0] == " " )
		inputValue = inputValue.substring(1, inputValue.length);
	
    top.MAX_RECIPIENTS++;

    var newNode = templateNode.cloneNode(true);
    parentNode.appendChild(newNode); // we need to insert the new node before we set the value of the select element!

    var input = newNode.getElementsByTagName('INPUT');
    if ( input && input.length == 1 )
    {
	    input[0].setAttribute("value", inputValue);
	    input[0].setAttribute("id", "msgRecipient#" + top.MAX_RECIPIENTS);
	}
    var select = newNode.getElementsByTagName('SELECT');
    if ( select && select.length == 1 )
    {
//Doesn't work!	    select[0].setAttribute("value", popupValue);
        select[0].value = popupValue;
	    select[0].setAttribute("id", "msgRecipientType#" + top.MAX_RECIPIENTS);
	}
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

function awNotAnEmptyArea(event)
{
	//This is temporary until i figure out how to ensure to always having an empty space after the last row
	dump("awNotAnEmptyArea\n");

	var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
	if ( lastInput && lastInput.value )
		awAppendNewRow(false);

	event.preventBubble();
}

function awClickEmptySpace(setFocus)
{
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

function awInputChanged(inputElement)
{
	dump("awInputChanged\n");
	AutoCompleteAddress(inputElement);

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
	    var lastRecipientType = awGetPopupElement(top.MAX_RECIPIENTS).value;

		newNode = awCopyNode(treeitem1, body, 0);
		top.MAX_RECIPIENTS++;

        var input = newNode.getElementsByTagName('INPUT');
        if ( input && input.length == 1 )
        {
    	    input[0].setAttribute("value", "");
    	    input[0].setAttribute("id", "msgRecipient#" + top.MAX_RECIPIENTS);
    	}
        var select = newNode.getElementsByTagName('SELECT');
        if ( select && select.length == 1 )
        {
//doesn't work!    	    select[0].setAttribute("value", lastRecipientType);
    	    select[0].value = lastRecipientType;
    	    select[0].setAttribute("id", "msgRecipientType#" + top.MAX_RECIPIENTS);
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