// functions needed from abMainWindow and abSelectAddresses

function AbNewCardDialog()
{
	window.openDialog("chrome://addressbook/content/abNewCardDialog.xul",
					  "",
					  "chrome,resizeable=no,modal",
					  {GetAddressBooksAndURIs:GetAddressBooksAndURIs});
}

function GetAddressBooksAndURIs(abArray, uriArray)
{
	var numAddressBooks = 0;
	var selected = 0;
	var tree = document.getElementById('dirTree');
	if ( tree )
	{
		var body = tree.getElementById('dirTreeBody')

		if ( body )
		{
			var treeitems = body.getElementsByTagName('treeitem');
			if ( treeitems )
			{
				var name, uri, item, selectedItem = 0;
				
				if ( tree.selectedItems && (tree.selectedItems.length == 1) )
					selectedItem = tree.selectedItems[0];

				for ( var index = 0; index < treeitems.length; index++ ) 
				{ 
					item = treeitems[index];
					uri = item.getAttribute('id');
					if ( item == selectedItem )
						selected = numAddressBooks;
					var buttons = item.getElementsByTagName('titledbutton');
					if ( uri && buttons && buttons.length == 1 )
					{
						name = buttons[0].getAttribute('value');
						if ( name )
						{
							abArray[numAddressBooks] = name;
							uriArray[numAddressBooks] = uri;
							numAddressBooks++;
						}
					}
				}
			}
		}
	}
	return selected;
}

function AbEditCardDialog(card, okCallback)
{
	var dialog = window.openDialog("chrome://addressbook/content/abEditCardDialog.xul",
								   "",
								   "chrome,resizeable=no,modal",
								   {abURI:document.getElementById('resultsTree').getAttribute('ref'),
								    card:card, okCallback:okCallback});
	
	return dialog;
}

function EditCard()
{
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var resultsTree = document.getElementById('resultsTree');

	if ( resultsTree.selectedItems && resultsTree.selectedItems.length == 1 )
	{
		var uri = resultsTree.selectedItems[0].getAttribute('id');
		var card = rdf.GetResource(uri);
		card = card.QueryInterface(Components.interfaces.nsIAbCard);
		AbEditCardDialog(card, UpdateCardView);
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
	if(!node)
		return false;

	var rdfCore = XPAppCoresManager.Find("RDFCore");
	if (!rdfCore)
	{
		rdfCore = new RDFCore();
		if (!rdfCore)
		{
			return(false);
		}

		rdfCore.Init("RDFCore");

	}

	// sort!!!
	sortDirection = "ascending";
    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
            sortDirection = "descending";
    else if (currentDirection == "descending")
            sortDirection = "ascending";
    else    sortDirection = "ascending";

    rdfCore.doSort(node, sortKey, sortDirection);

    return(true);


}


