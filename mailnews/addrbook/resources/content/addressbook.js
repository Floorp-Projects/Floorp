function AbNewCardDialog()
{
	var myURI = GetResultTreeDirectory().getAttribute('ref');
	dump("myURI = " + myURI + "\n");
	
	var dialog = window.openDialog("chrome://addressbook/content/newcardDialog.xul",
								   "abNewCard",
								   "chrome",
								   {abURI:GetResultTreeDirectory().getAttribute('ref')});

	return dialog;
}

function AbEditCardDialog(card, okCallback)
{
	var dialog = window.openDialog("chrome://addressbook/content/editcardDialog.xul",
								   "abEditCard",
								   "chrome",
								   {abURI:GetResultTreeDirectory().getAttribute('ref'),
								    card:card, okCallback:okCallback});
	
	return dialog;
}

var addressbook = Components.classes["component://netscape/addressbook"].createInstance();
	addressbook = addressbook.QueryInterface(Components.interfaces.nsIAddressBook);

function AbDelete()
{
	dump("\AbDelete from XUL\n");
	var tree = GetResultTree();
	if( tree )
	{
		dump("tree is valid\n");
		//get the selected elements
		var cardList = tree.getElementsByAttribute("selected", "true");
		//get the current folder
		var srcDirectory = GetResultTreeDirectory();
		dump("srcDirectory = " + srcDirectory + "\n");
		addressbook.DeleteCards(tree, srcDirectory, cardList);
	}
}


function GetDirectoryTree()
{
	var directoryTree = frames["directoryFrame"].document.getElementById('dirTree');
	dump("directoryTree = " + directoryTree + "\n");
	return directoryTree;
}

function GetResultTree()
{
	var cardTree = frames["resultsFrame"].document.getElementById('resultTree');
	dump("cardTree = " + cardTree + "\n");
	return cardTree;
}

function GetResultTreeDirectory()
{
	return(GetResultTree());
}


function EditCard()
{
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var resultsDoc = frames["resultsFrame"].document;
	var selArray = resultsDoc.getElementsByAttribute('selected', 'true');

	if ( selArray && selArray.length == 1 )
	{
		var uri = selArray[0].getAttribute('id');
		var card = rdf.GetResource(uri);
		card = card.QueryInterface(Components.interfaces.nsIAbCard);
		AbEditCardDialog(card, UpdateCardView);
	}
}

function UpdateCardView()
{
	var resultsDoc = frames["resultsFrame"].document;
	var selArray = resultsDoc.getElementsByAttribute('selected', 'true');
	dump("UpdateCardView -- selArray = " + selArray + "\n");

	if ( selArray && selArray.length == 1 )
		DisplayCardViewPane(selArray[0]);
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
	
	var resultsDoc = frames["resultsFrame"].document;
	var bucketDoc = frames["directoryFrame"].document;
	
	rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var selArray = resultsDoc.getElementsByAttribute('selected', 'true');
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

function AbClose()
{
	top.window.close();
}

function AbNewAddressBook()
{
    var dirTree = GetDirectoryTree(); 
	var srcDirectory = GetResultTreeDirectory();
	addressbook.NewAddressBook(dirTree.database, srcDirectory, "My New Address Book");
}


