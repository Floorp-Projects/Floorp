var importService = 0;
var	fieldMap = null;
var transferType = null;
var recordNum = 0;
var amAtEnd = false;
var addInterface = null;
var dialogResult = null;
var dragStart = false;
var dragData = null;

function OnLoadFieldMapImport()
{
	// top.bundle = srGetStrBundle("chrome://messenger/locale/importMsgs.properties");
	top.importService = Components.classes["@mozilla.org/import/import-service;1"].getService();
	top.importService = top.importService.QueryInterface(Components.interfaces.nsIImportService);
	top.transferType = "moz/fieldmap";
	top.recordNum = 0;
	
	// We need a field map object...
	// assume we have one passed in? or just make one?
	if (window.arguments && window.arguments[0]) {
		top.fieldMap = window.arguments[0].fieldMap;
		top.addInterface = window.arguments[0].addInterface;
		top.dialogResult = window.arguments[0].result;
	}
	if (top.fieldMap == null) {
		top.fieldMap = top.importService.CreateNewFieldMap();	
		top.fieldMap.DefaultFieldMap( top.fieldMap.numMozFields);
	}
	
	doSetOKCancel( FieldImportOKButton, 0);

	ListFields();
	OnNextRecord();
}

function SetDivText(id, text)
{
	var div = document.getElementById(id);
	
	if ( div )
	{
		if ( div.childNodes.length == 0 )
		{
			var textNode = document.createTextNode(text);
			div.appendChild(textNode);                   			
		}
		else if ( div.childNodes.length == 1 )
			div.childNodes[0].nodeValue = text;
	}
}


function FieldSelectionChanged()
{
	var tree = document.getElementById('fieldList');
	if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
	{
	}
}


function IndexInMap( index)
{
	var count = top.fieldMap.mapSize;
	var	i;
	for (i = 0; i < count; i++) {
		if (top.fieldMap.GetFieldMap( i) == index)
			return( true);
	}

	return( false);
}

function ListFields() {
	if (top.fieldMap == null)
		return;
		
	body = document.getElementById("fieldBody");
	count = top.fieldMap.mapSize;
	var index;
	var	i;
	for (i = 0; i < count; i++) {
		index = top.fieldMap.GetFieldMap( i);
		AddFieldToList( body, top.fieldMap.GetFieldDescription( index), index, top.fieldMap.GetFieldActive( i));
	}

	count = top.fieldMap.numMozFields;
	for (i = 0; i < count; i++) {
		if (!IndexInMap( i))
			AddFieldToList( body, top.fieldMap.GetFieldDescription( i), i, false);
	}
}


function CreateField( name, index, on, cBoxIndex)
{
	var item = document.createElement('treeitem');
	var row = document.createElement('treerow');
	var cell = document.createElement('treecell');
	cell.setAttribute('value', name);
	item.setAttribute('field-index', index);
	
	var cCell = document.createElement( 'treecell');
	var cBox = document.createElement( 'checkbox');
	if (on == true)
		cBox.setAttribute( 'checked', "true");
	cBox.setAttribute( 'value', name);

	cCell.appendChild( cBox);
	cCell.setAttribute( 'allowevents', "true");	

	row.appendChild( cCell);
	/* row.appendChild(cell); */

	cell = document.createElement( 'treecell');
	cell.setAttribute( "class", "importsampledata");
	cell.setAttribute( 'value', " ");
	cell.setAttribute( 'noDrag', "true");	
		
	row.appendChild( cell);

	item.appendChild(row);

	return( item);
}

function AddFieldToList(body, name, index, on)
{
	var item = CreateField( name, index, on, body.childNodes.length);
	body.appendChild(item);
}

function BeginDrag( event)
{
	top.dragStart = false;

	var tree = document.getElementById("fieldList");
	if ( event.target == tree ) {
		return( true);					// continue propagating the event
    }

	if (!tree) {
		return( false);
	}

	var dragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
	if ( dragService ) dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if ( !dragService )	{
		return(false);
	}

	var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance();
	if ( trans ) trans = trans.QueryInterface(Components.interfaces.nsITransferable);
	if ( !trans ) {
		return(false);
	}

	var genData = Components.classes["@mozilla.org/supports-wstring;1"].createInstance();
	if ( genData ) genData = genData.QueryInterface(Components.interfaces.nsISupportsWString);
	if (!genData) {
		return(false);
	}

    // trans.addDataFlavor( "text/unicode");
    trans.addDataFlavor( top.transferType);

	// the index is on the <treeitem> which is two levels above the <treecell> which is
	// the target of the event.
	if (event.target.getAttribute( 'noDrag') == "true") {
		return( false);
	}

	var index = event.target.parentNode.parentNode.getAttribute("field-index");
	if (!index)
		index = event.target.parentNode.parentNode.parentNode.getAttribute( "field-index");
	if (!index)
		return( false);

	var indexStr = ("" + index);
	genData.data = indexStr;
	
	// trans.setTransferData ( "text/unicode", genData, indexStr.length * 2);
	trans.setTransferData ( top.transferType, genData, indexStr.length * 2);

	var transArray = Components.classes["@mozilla.org/supports-array;1"].createInstance();
	if ( transArray ) transArray = transArray.QueryInterface(Components.interfaces.nsISupportsArray);
	if ( !transArray )	{
		return(false);
	}

	// put it into the transferable as an |nsISupports|
	var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
	transArray.AppendElement(genTrans);
	
	var nsIDragService = Components.interfaces.nsIDragService;
	top.dragStart = true;

	dragService.invokeDragSession ( event.target, transArray, null, nsIDragService.DRAGDROP_ACTION_MOVE);
	

	return( false);  // don't propagate the event if a drag has begun
}


function SetRow( row, dstIndex, dstBox, dstField)
{
	row.setAttribute( 'field-index', dstIndex);
	if (dstBox == true) {
		row.firstChild.firstChild.firstChild.checked = true;
	}
	else {
		row.firstChild.firstChild.firstChild.checked = false;
	}

	/* row.firstChild.childNodes[1].setAttribute( "value", dstField); */

	row.firstChild.firstChild.firstChild.setAttribute( 'value', dstField);
}


function AssignRow( toRow, fromRow)
{
	/*
	SetRow( toRow,	fromRow.getAttribute( 'field-index'), 
					fromRow.firstChild.firstChild.firstChild.checked,
					fromRow.firstChild.childNodes[1].getAttribute( "value"));
	*/
	SetRow( toRow,	fromRow.getAttribute( 'field-index'), 
					fromRow.firstChild.firstChild.firstChild.checked,
					fromRow.firstChild.firstChild.firstChild.getAttribute( "value"));
}


function FindRowFromIndex( body, index)
{
	for (var i = 0; i < body.childNodes.length; i++) {
		if (body.childNodes[i].getAttribute( 'field-index') == index)
			return( i);
	}

	return( -1);
}

function FindRowFromItem( body, item)
{
	for (var i = 0; i < body.childNodes.length; i++) {
		if (body.childNodes[i] == item)
			return( i);
	}

	return( -1);
}

function DropOnTree( event)
{

	var treeRoot = document.getElementById("fieldList");
	if (!treeRoot)	return(false);

	// target is the <treecell>, and the <treeitem> is two levels above
	var treeItem = event.target.parentNode.parentNode;
	if (!treeItem)	return(false);

	// get drop hint attributes
	var dropBefore = treeItem.getAttribute("dd-droplocation");
	var dropOn = treeItem.getAttribute("dd-dropon");

	// calculate drop action
	var dropAction;
	if (dropBefore == "true")	dropAction = "before";
	else if (dropOn == "true")	dropAction = "on";
	else				dropAction = "after";

	dump( "DropAction: " + dropAction + "\n");

	// calculate parent container node
	/* bookmarks.js uses this, not sure what it's for???
	var containerItem = treeItem;
	if (dropAction != "on")
		containerItem = treeItem.parentNode.parentNode;
	*/

	var dragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
	if ( dragService ) dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if ( !dragService )	return(false);
	
	var dragSession = dragService.getCurrentSession();
	if ( !dragSession )	return(false);

	var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance();
	if ( trans ) trans = trans.QueryInterface(Components.interfaces.nsITransferable);
	if ( !trans )		return(false);
	trans.addDataFlavor( top.transferType);
	// trans.addDataFlavor( "text/unicode");

	var body = document.getElementById( "fieldBody");
	if (!body)
		return( false);

	for ( var i = 0; i < dragSession.numDropItems; ++i )
	{
		dragSession.getData ( trans, i );
		var dataObj = new Object();
		var bestFlavor = new Object();
		var len = new Object();
		try {
			trans.getAnyTransferData( bestFlavor, dataObj, len);
			if ( dataObj )	{
				dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
			}
			if ( !dataObj )	{
				continue;
			}

			var fIndex = parseInt( dataObj.data);
			
			dump( "Source row: " + fIndex + "\n");

			// so now what, move the given row to the new position!
			// find the source row index
			var srcRow = FindRowFromIndex( body, fIndex);
			if (srcRow < 0) {
				dump( "*** Error finding source row\n");
				continue;
			}
			var dstRow = FindRowFromItem( body, treeItem);
			if (dstRow < 0) {
				dump( "*** Error finding destination row\n");
				continue;
			}

			// always do before unless we can't
			if (dropAction == "on" || dropAction == "after") {
				dstRow++;
				if (dstRow >= body.childNodes.length) {
					if (srcRow == (body.childNodes.length - 1))
						continue;
					dstRow = -1;
				}
			}
			
			var maxIndex = body.childNodes.length - 1;
			var dstBox = body.childNodes[srcRow].firstChild.firstChild.firstChild.checked;
			var dstField = body.childNodes[srcRow].firstChild.firstChild.firstChild.getAttribute( 'value');
			var dstIndex = body.childNodes[srcRow].getAttribute( 'field-index');
			
			dump( "FieldDrag from " + srcRow + " to " + dstRow + "\n");

			if (dstRow < 0) {
				// remove the row and append it to the end!
				// Move srcRow to the end!				
				while (srcRow < maxIndex) {
					AssignRow( body.childNodes[srcRow], body.childNodes[srcRow + 1]);
					srcRow++;
				}
				
				SetRow( body.childNodes[maxIndex], dstIndex, dstBox, dstField);
			}
			else {
				if (dstRow == srcRow)
					continue;
				if (srcRow < dstRow)
					dstRow--;
				if (dstRow == srcRow)
					continue;

				if (dstRow < srcRow) {
					// move dstRow down to srcRow
					while (dstRow < srcRow) {
						AssignRow( body.childNodes[srcRow], body.childNodes[srcRow - 1]);
						srcRow--;
					}
				}
				else {
					// move dstRow up to srcRow
					while (srcRow < dstRow) {
						AssignRow( body.childNodes[srcRow], body.childNodes[srcRow + 1]);
						srcRow++;
					}
				}
				SetRow( body.childNodes[dstRow], dstIndex, dstBox, dstField);
			}

		}
		catch( ex) {
			dump( "Caught drag exception in DropOnTree\n");
			dump( ex);
			dump( "\n");
		}
	}

	return(false);
}


function DragOverTree( event)
{
	if (!top.dragStart)
		return( false);

	var validFlavor = false;
	var dragSession = null;
	var retVal = true;

	var dragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
	if ( dragService ) dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if ( !dragService )	return(false);

	dragSession = dragService.getCurrentSession();
	if ( !dragSession )	return(false);

	if ( dragSession.isDataFlavorSupported( top.transferType) )	validFlavor = true;
	// if ( dragSession.isDataFlavorSupported( "text/unicode") )	validFlavor = true;
	
	if (event.target == document.getElementById( "fieldBody")) return( false);

	// touch the attribute on the rowgroup to trigger the repaint with the drop feedback.
	if ( validFlavor )
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

function ShowSampleData( data)
{
	var fBody = document.getElementById( "fieldBody");
	var fields = data.split( "\n");
	for (var i = 0; i < fBody.childNodes.length; i++) {
		if (i < fields.length) {
			// fBody.childNodes[i].firstChild.childNodes[2].setAttribute( 'value', fields[i]);
			fBody.childNodes[i].firstChild.childNodes[1].setAttribute( 'value', fields[i]);
		}
		else {
			// fBody.childNodes[i].firstChild.childNodes[2].setAttribute( 'value', " ");
			fBody.childNodes[i].firstChild.childNodes[1].setAttribute( 'value', " ");
		}
	}

}

function FetchSampleData()
{
	if (!top.addInterface)
		return( false);
	
	var num = top.recordNum - 1;
	if (num < 0)
		num = 0;
	var data = top.addInterface.GetData( "sampleData-"+num);
	var result = false;
	if (data != null) {
		data = data.QueryInterface( Components.interfaces.nsISupportsWString);
		if (data != null) {
			ShowSampleData( data.data);	
			result = true;
		}
	}
	
	return( result);
}

function OnPreviousRecord()
{
	if (top.recordNum <= 1)
		return;
	top.recordNum--;
	top.amAtEnd = false;
	if (FetchSampleData()) {
		document.getElementById('recordNumber').setAttribute('value', ("" + top.recordNum));		
	}
}

function OnNextRecord()
{
	if (top.amAtEnd)
		return;
	top.recordNum++;
	if (!FetchSampleData()) {
		top.amAtEnd = true;
		top.recordNum--;
	}
	else
		document.getElementById('recordNumber').setAttribute('value', ("" + top.recordNum));		
}

function FieldImportOKButton()
{
	var body = document.getElementById( "fieldBody");
	var max = body.childNodes.length;
	var fIndex;
	var on;
	for (var i = 0; i < max; i++) {
		fIndex = body.childNodes[i].getAttribute( 'field-index');
		on = body.childNodes[i].firstChild.firstChild.firstChild.checked;
		top.fieldMap.SetFieldMap( i, fIndex);
		if (on == true)
			top.fieldMap.SetFieldActive( i, true);
		else
			top.fieldMap.SetFieldActive( i, false);
	}

	top.dialogResult.ok = true;

	return true;
}
