function AbNewCardDialog()
{
	var dialog = window.openDialog("chrome://addressbook/content/newcardDialog.xul",
								   "abNewCard",
								   "chrome",
								   {abURI:GetResultTreeDirectory()});

	return dialog;
}

function AbEditCardDialog(card, okCallback)
{
	var dialog = window.openDialog("chrome://addressbook/content/editcardDialog.xul",
								   "abEditCard",
								   "chrome",
								   {abURI:GetResultTreeDirectory(),
								    card:card, okCallback:okCallback});
	
	return dialog;
}

function AbDelete()
{
	var addressbook = Components.classes["component://netscape/addressbook"].createInstance();
	addressbook = addressbook.QueryInterface(Components.interfaces.nsIAddressBook);
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
	return directoryTree;
}

function GetResultTree()
{
	var cardTree = frames["resultsFrame"].document.getElementById('resultTree');
	return cardTree;
}

function GetResultTreeDirectory()
{
	var tree = GetResultTree();
	var treechildrenList = tree.getElementsByTagName('treechildren');
	
	if ( treechildrenList.length == 1 )
		return(treechildrenList[0]);
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

