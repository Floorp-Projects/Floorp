// functions needed from abMainWindow and abSelectAddresses

function AbNewCard()
{
	var selectedAB = 0;
	var tree = document.getElementById('dirTree');
	if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
		selectedAB = tree.selectedItems[0].getAttribute('id');
		
	goNewCardDialog(selectedAB);
}

function AbEditCard()
{
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var resultsTree = document.getElementById('resultsTree');

	if ( resultsTree.selectedItems && resultsTree.selectedItems.length == 1 )
	{
		var uri = resultsTree.selectedItems[0].getAttribute('id');
		var card = rdf.GetResource(uri);
		card = card.QueryInterface(Components.interfaces.nsIAbCard);
		var editCardCallback = 0;
		if ( top.addressbook && top.addressbook.editCardCallback )
			editCardCallback = top.addressbook.editCardCallback;
		goEditCardDialog(document.getElementById('resultsTree').getAttribute('ref'),
						 card, editCardCallback);
	}
}

function AbNewMessage()
{
	var msgComposeService = Components.classes["component://netscape/messengercompose"].getService(); 
	msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService); 

	msgComposeService.OpenComposeWindowWithValues(null, 0, GetSelectedAddresses(), null, null,
												  null, null, null); 
}  

function GetSelectedAddresses()
{
	var item, uri, rdf, cardResource, card;
	var selectedAddresses = "";
	
	var resultsTree = document.getElementById('resultsTree');
	
	rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	if ( resultsTree.selectedItems && resultsTree.selectedItems.length )
	{
		for ( item = 0; item < resultsTree.selectedItems.length; item++ )
		{
			uri = resultsTree.selectedItems[item].getAttribute('id');
			cardResource = rdf.GetResource(uri);
			card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
			if ( selectedAddresses )
				selectedAddresses += ",";
			selectedAddresses += "\"" + card.DisplayName + "\" <" + card.PrimaryEmail + ">";
		}
	}
	dump("selectedAddresses = " + selectedAddresses + "\n");
	return selectedAddresses;	
}


function ChangeDirectoryByDOMNode(dirNode)
{
	var uri = dirNode.getAttribute('id');
	dump("uri = " + uri + "\n");
	ChangeDirectoryByURI(uri);
}

function ChangeDirectoryByURI(uri)
{
	var tree = document.getElementById('resultsTree');
	if ( tree )
		tree.setAttribute('ref', uri);
}

function ResultsPaneSelectionChange()
{
	// FIX ME! - Should use some js var to determine abmain vs selectaddress dialog
	// not in ab window if no parent.parent.rdf
	if ( parent.parent.rdf )
	{
		var tree = document.getElementById('resultsTree');
		
		if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
			DisplayCardViewPane(tree.selectedItems[0]);
		else
			ClearCardViewPane();
	}

}

function SortResultPane(column, sortKey)
{
	var node = document.getElementById(column);
	if(!node)    return(false);

	var isupports = Components.classes["component://netscape/rdf/xul-sort-service"].getService();
	if (!isupports)    return(false);
	
	var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
	if (!xulSortService)    return(false);

	// sort!!!
	sortDirection = "ascending";
    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
            sortDirection = "descending";
    else if (currentDirection == "descending")
            sortDirection = "ascending";
    else    sortDirection = "ascending";

	xulSortService.Sort(node, sortKey, sortDirection);

    return(true);
}
