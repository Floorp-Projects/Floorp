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

function MailListOKButton()
{
	var popup = document.getElementById('abPopup');
	if ( popup )
	{
		var uri = popup.getAttribute('data');
		
		// FIX ME - hack to avoid crashing if no ab selected because of blank option bug from template
		// should be able to just remove this if we are not seeing blank lines in the ab popup
		if ( !uri )
			return false;  // don't close window
		// -----
		
		//Add mailing list to database
		var mailList = Components.classes["component://netscape/addressbook/directoryproperty"].createInstance();
		mailList = mailList.QueryInterface(Components.interfaces.nsIAbDirectory);

		mailList.listName = document.getElementById('ListName').value;

		if (mailList.listName.length == 0)
			return false;

		mailList.listNickName = document.getElementById('ListNickName').value;
		mailList.description = document.getElementById('ListDescription').value;
		
		var i = 1;
	    while ((inputField = awGetInputElement(i)))
	    {
	    	fieldValue = inputField.value;
	    	if (fieldValue != "")
	    	{
				var cardproperty = Components.classes["component://netscape/addressbook/cardproperty"].createInstance();
				if (cardproperty)
				{
					cardproperty = cardproperty.QueryInterface(Components.interfaces.nsIAbCard);
					if (cardproperty)
					{
						cardproperty.primaryEmail = fieldValue
						mailList.addressLists.AppendElement(cardproperty);
					}
				}
			}
	    	i++;
	    }
		mailList.addMailListToDatabase(uri);
	}	
	
	return true;	// close the window
}

function OnLoadMailList()
{
	doSetOKCancel(MailListOKButton, 0);
	
	var selectedAB;
	if (window.arguments && window.arguments[0])
	{
		if ( window.arguments[0].selectedAB )
			selectedAB = window.arguments[0].selectedAB;
		else
			selectedAB = "abdirectory://abook.mab";
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
				if ( menupopup.childNodes[index].getAttribute('data') == selectedAB )
				{
					abPopup.value = menupopup.childNodes[index].getAttribute('value');
					abPopup.data = menupopup.childNodes[index].getAttribute('data');
					break;
				}
			}
		}
	}

//	GetCardValues(editCard.card, document);

	//// FIX ME - looks like we need to focus on both the text field and the tab widget
	//// probably need to do the same in the addressing widget
	
	// focus on first name
	var listName = document.getElementById('ListName');
	if ( listName )
		listName.focus();
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
	dump("***** awReturnHit\n");
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
//	AutoCompleteAddress(inputElement);

	//Do we need to add a new row?
	var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
	if ( lastInput && lastInput.value && !top.doNotCreateANewRow)
		awAppendNewRow(false);
	top.doNotCreateANewRow = false;
}

function awInputElementName()
{
    if (inputElementType == "")
        inputElementType = document.getElementById("address#1").nodeName;
    return inputElementType;
}

function awAppendNewRow(setFocus)
{
dump("-----awAppendNewRow\n");
	var body = document.getElementById('addressList');
	var treeitem1 = awGetTreeItem(1);
	
	if ( body && treeitem1 )
	{
		newNode = awCopyNode(treeitem1, body, 0);
		top.MAX_RECIPIENTS++;

        var input = newNode.getElementsByTagName(awInputElementName());
        if ( input && input.length == 1 )
        {
dump("-----awAppendNewRow 1\n");
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

function DropOnAddressListTree(event)
{
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
		
		if (card.isMailList)
			DropListAddress(card.name); 
		else
			DropListAddress(card.primaryEmail); 
		
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
