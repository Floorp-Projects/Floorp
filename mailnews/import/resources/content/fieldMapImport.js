var importService = 0;
var	fieldMap = null;
var transferType = null;
var recordNum = 0;
var amAtEnd = false;
var addInterface = null;
var dialogResult = null;

function OnLoadFieldMapImport()
{
	// top.bundle = srGetStrBundle("chrome://messenger/locale/importMsgs.properties");
	top.importService = Components.classes["component://mozilla/import/import-service"].createInstance();
	top.importService = top.importService.QueryInterface(Components.interfaces.nsIImportService);
	top.transferType = "text/plain";
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


function CheckClick( item)
{
	if (item.getAttribute( 'isChecked') == "true") {
		item.setAttribute( 'isChecked', "false");
		item.firstChild.removeAttribute( 'checked');
	}
	else {
		item.setAttribute( 'isChecked', "true");
		item.firstChild.setAttribute( 'checked', "true");
	}
	
	return( true);
}


function CreateField( name, index, on)
{
	var item = document.createElement('treeitem');
	var row = document.createElement('treerow');
	var cell = document.createElement('treecell');
	cell.setAttribute('value', name);
	item.setAttribute('field-index', index);
	
	var cCell = document.createElement( 'treecell');
	var cBox = document.createElement( 'html:input');
	cBox.setAttribute( 'type', "checkbox");
	if (on == true)
		cBox.setAttribute( 'checked', "true");
	cBox.setAttribute( 'onclick', "return BoxClick();");

	cCell.appendChild( cBox);	
	cCell.setAttribute( 'onclick', "return CheckClick( event.target);");
	cCell.setAttribute( 'isChecked', on);

	row.appendChild( cCell);


	row.appendChild(cell);
	item.appendChild(row);

	return( item);
}

function AddFieldToList(body, name, index, on)
{
	var item = CreateField( name, index, on);
	body.appendChild(item);
}

function BeginDrag( event)
{
	var tree = document.getElementById("fieldList");
	if ( event.target == tree )
		return( true);					// continue propagating the event
    
	if (!tree)
		return( false);
    
	var dragService = Components.classes["component://netscape/widget/dragservice"].getService();
	if ( dragService ) dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if ( !dragService )	return(false);

	var trans = Components.classes["component://netscape/widget/transferable"].createInstance();
	if ( trans ) trans = trans.QueryInterface(Components.interfaces.nsITransferable);
	if ( !trans )		return(false);

	var genData = Components.classes["component://netscape/supports-string"].createInstance();
	if ( genData ) genData = genData.QueryInterface(Components.interfaces.nsISupportsString);
	if (!genData)		return(false);

    trans.addDataFlavor( top.transferType);
        
	// the index is on the <treeitem> which is two levels above the <treecell> which is
	// the target of the event.
	var index = event.target.parentNode.parentNode.getAttribute("field-index");
	var indexStr = ("" + index);
	genData.data = indexStr;

	trans.setTransferData ( top.transferType, genData, indexStr.length);

	var transArray = Components.classes["component://netscape/supports-array"].createInstance();
	if ( transArray ) transArray = transArray.QueryInterface(Components.interfaces.nsISupportsArray);
	if ( !transArray )	return(false);

	// put it into the transferable as an |nsISupports|
	var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
	transArray.AppendElement(genTrans);
	
	var nsIDragService = Components.interfaces.nsIDragService;
	dragService.invokeDragSession ( transArray, null, nsIDragService.DRAGDROP_ACTION_MOVE);

	return( false);  // don't propagate the event if a drag has begun
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

	var dragService = Components.classes["component://netscape/widget/dragservice"].getService();
	if ( dragService ) dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if ( !dragService )	return(false);
	
	var dragSession = dragService.getCurrentSession();
	if ( !dragSession )	return(false);

	var trans = Components.classes["component://netscape/widget/transferable"].createInstance();
	if ( trans ) trans = trans.QueryInterface(Components.interfaces.nsITransferable);
	if ( !trans )		return(false);
	trans.addDataFlavor( top.transferType);

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
				dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
			}
			if ( !dataObj )	{
				continue;
			}
			var fIndex = parseInt( dataObj.data);

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
			
			var name;
			var	on;

			if (dstRow < 0) {
				// remove the row and append it to the end!
				var dstItem = body.childNodes[srcRow];
				body.removeChild( dstItem);
				body.appendChild( dstItem);

			}
			else {
				if (dstRow == srcRow)
					continue;

				// insert the row before dstRow
				var dstItem = body.childNodes[srcRow];
				// name = dstItem.firstChild.firstChild.getAttribute( 'value');
				// on = dstItem.getAttribute( 'isChecked');
				body.removeChild( dstItem);
				if (srcRow < dstRow)
					dstRow--;
				// dstItem = CreateField( name, fIndex, on);
				body.insertBefore( dstItem, body.childNodes[dstRow]);
			}

		}
		catch( ex) {
		}
	}

	return(false);
}


function DragOverTree( event)
{
	var validFlavor = false;
	var dragSession = null;
	var retVal = true;

	var dragService = Components.classes["component://netscape/widget/dragservice"].getService();
	if ( dragService ) dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if ( !dragService )	return(false);

	dragSession = dragService.getCurrentSession();
	if ( !dragSession )	return(false);

	if ( dragSession.isDataFlavorSupported( top.transferType) )	validFlavor = true;
	
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
	var body = document.getElementById( "dataBody");
	var max = body.childNodes.length - 1;
	while (max >= 0) {
		body.removeChild( body.childNodes[max]);
		max--;
	}
	
	// split up the string into individual str's for the tree nodes
	var fields = data.split( "\n");
	for (var i = 0; i < fields.length; i++) {
		var item = document.createElement('treeitem');
		var row = document.createElement('treerow');
		var cell = document.createElement('treecell');
		cell.setAttribute('value', fields[i]);
		row.appendChild(cell);
		item.appendChild(row);
		body.appendChild(item);
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
		SetDivText( "recordNumber", ("" + top.recordNum));		
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
		SetDivText( "recordNumber", ("" + top.recordNum));		
}

function FieldImportOKButton()
{
	var body = document.getElementById( "fieldBody");
	var max = body.childNodes.length;
	var fIndex;
	var on;
	for (var i = 0; i < max; i++) {
		fIndex = body.childNodes[i].getAttribute( 'field-index');
		on = body.childNodes[i].firstChild.firstChild.getAttribute( 'isChecked');
		top.fieldMap.SetFieldMap( i, fIndex);
		if (on == "true")
			top.fieldMap.SetFieldActive( i, true);
		else
			top.fieldMap.SetFieldActive( i, false);
	}

	top.dialogResult.ok = true;

	return true;
}
