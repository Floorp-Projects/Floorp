/* -*- Mode: Java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

function debugDump(msg)
{
  // uncomment for noise
   dump(msg+"\n");
}

function GetDragService()
{
	var dragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
	if (dragService) 
		dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);

	return dragService;
}

function GetRDFService()
{
	var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
	if (rdf)   
		rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	return rdf;
}

function DragOverTree(event)
{
	var validFlavor = false;
	var dragSession = null;
	var retVal = true;

	var dragService = GetDragService();
	if ( !dragService )	return(false);

	dragSession = dragService.getCurrentSession();
	if ( !dragSession )	return(false);

	if ( dragSession.isDataFlavorSupported("text/nsabcard") )	validFlavor = true;
	//XXX other flavors here...

	// touch the attribute on the rowgroup to trigger the repaint with the drop feedback.
	if ( validFlavor )
	{
		//XXX this is really slow and likes to refresh N times per second.
		var treeItem = event.target.parentNode.parentNode;
		treeItem.setAttribute ( "dd-triggerrepaint", 0 );
		dragSession.canDrop = true;
		// necessary??
		retVal = false; // do not propagate message
	}
	return(retVal);
}

function BeginDragResultTree(event)
{
	debugDump("BeginDragResultTree\n");

	//XXX we rely on a capturer to already have determined which item the mouse was over
	//XXX and have set an attribute.
    
	// if the click is on the tree proper, ignore it. We only care about clicks on items.

    if (event.target.localName != "treecell" &&
        event.target.localName != "treeitem")
        return false;

	var tree = resultsTree;
	if ( event.target == tree )
		return(true);					// continue propagating the event
    
	var childWithDatabase = tree;
	if ( ! childWithDatabase )
		return(false);

	var database = childWithDatabase.database;
	var rdf = GetRDFService();
	if ((!rdf) || (!database))	{ debugDump("CAN'T GET DATABASE\n"); return(false); }
		        
	var dragStarted = false;

	var dragService = GetDragService();
	if ( !dragService )	return(false);

	var transArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
	if ( !transArray ) return(false); 

	var selArray = tree.selectedItems;
	var count = selArray.length;
	debugDump("selArray.length = " + count + "\n");
	for ( var i = 0; i < count; ++i )
	{
		var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
		if ( !trans )		return(false);

		var genTextData = Components.classes["@mozilla.org/supports-wstring;1"].createInstance(Components.interfaces.nsISupportsWString);
		if (!genTextData)	return(false);

		trans.addDataFlavor("text/nsabcard");
        
		// get id (url) 
		var id = selArray[i].getAttribute("id");
		genTextData.data = id;
		debugDump("    ID #" + i + " = " + id + "\n");

		trans.setTransferData ( "text/nsabcard", genTextData, id.length * 2 );  // doublebyte byte data

		// put it into the transferable as an |nsISupports|
		var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
		transArray.AppendElement(genTrans);
	}

	var nsIDragService = Components.interfaces.nsIDragService;
	dragService.invokeDragSession ( event.target, transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
		nsIDragService.DRAGDROP_ACTION_MOVE );
	dragStarted = true;

	return(!dragStarted);  // don't propagate the event if a drag has begun
}

function DropOnDirectoryTree(event)
{
	debugDump("DropOnTree\n");

    if (event.target.localName != "treecell" &&
        event.target.localName != "treeitem")
        return false;

	var RDF = GetRDFService();
	if (!RDF) return(false);

	var treeRoot = dirTree;
	if (!treeRoot)	return(false);
	var treeDatabase = treeRoot.database;
	if (!treeDatabase)	return(false);

	// target is the <treecell>, and "id" is on the <treeitem> two levels above
	var treeItem = event.target.parentNode.parentNode;
	if (!treeItem)	return(false);

	// drop action is always "on" not "before" or "after"
	// get drop hint attributes
	var dropBefore = treeItem.getAttribute("dd-droplocation");
	var dropOn = treeItem.getAttribute("dd-dropon");

	var dropAction;
	if (dropOn == "true") 
		dropAction = "on";
	else
		return(false);

	var targetID = treeItem.getAttribute("id");
	if (!targetID)	return(false);

	debugDump("***targetID = " + targetID + "\n");

	var dragService = GetDragService();
	if ( !dragService )	return(false);
	
	var dragSession = dragService.getCurrentSession();
	if ( !dragSession )	return(false);

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
		if ( dataObj )	dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
		if ( !dataObj )	continue;

		// pull the URL out of the data object
		var sourceID = dataObj.data.substring(0, len.value);
		if (!sourceID)	continue;

		debugDump("    Node #" + i + ": drop '" + sourceID + "' " + dropAction + " '" + targetID + "'");
		debugDump("\n");

		sourceNode = RDF.GetResource(sourceID, true);
		if (!sourceNode)
			continue;
		
		var targetNode = RDF.GetResource(targetID, true);
		if (!targetNode) 
			continue;

		// Prevent dropping of a node before, after, or on itself
		if (sourceNode == targetNode)	continue;

		if (sourceID.substring(0,targetID.length) != targetID)
		{
			var cardResource = rdf.GetResource(sourceID);
			var card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
			if (card.isMailList == false)
				card.dropCardToDatabase(targetID);
		}
	}

	return(false);
}

function DropOnResultTree(event)
{
	debugDump("DropOnResultTree\n");
	return false;
}
