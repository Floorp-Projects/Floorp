// functions needed from abMainWindow and abSelectAddresses

function AbNewCardDialog()
{
	window.openDialog("chrome://addressbook/content/abNewCardDialog.xul",
					  "",
					  "chrome",
					  {GetAddressBooksAndURIs:GetAddressBooksAndURIs});
}

function GetAddressBooksAndURIs(abArray, uriArray)
{
	var numAddressBooks = 0;
	var selected = 0;
	var body = document.getElementById('dirTreeBody')

	if ( body )
	{
		var treeitems = body.getElementsByTagName('treeitem');
		if ( treeitems )
		{
			var name, uri, item;
			
			for ( var index = 0; index < treeitems.length; index++ ) 
			{ 
				item = treeitems[index];
				uri = item.getAttribute('id');
				if ( item.getAttribute('selected') )
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
	return selected;
}

function AbEditCardDialog(card, okCallback)
{
	var dialog = window.openDialog("chrome://addressbook/content/abEditCardDialog.xul",
								   "",
								   "chrome",
								   {abURI:document.getElementById('resultsTree').getAttribute('ref'),
								    card:card, okCallback:okCallback});
	
	return dialog;
}

function EditCard()
{
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var resultsTree = document.getElementById('resultsTree');
	var selArray = resultsTree.getElementsByAttribute('selected', 'true');

	if ( selArray && selArray.length == 1 )
	{
		var uri = selArray[0].getAttribute('id');
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

	var selArray = resultsTree.getElementsByAttribute('selected', 'true');
	if ( selArray && selArray.length )
	{
		for ( item = 0; item < selArray.length; item++ )
		{
			uri = selArray[item].getAttribute('id');
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
		
		var selArray = tree.getElementsByAttribute('selected', 'true');
		if ( selArray && (selArray.length == 1) )
			DisplayCardViewPane(selArray[0]);
		else
			ClearCardViewPane();
	}
}

