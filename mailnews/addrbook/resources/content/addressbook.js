// functions used only by abMainWindow

function OnLoadAddressBook()
{
	// FIX ME - later we will be able to use onload from the overlay
	OnLoadCardView();
}


// FIX ME - this should be in onload and have a different name
var addressbook = Components.classes["component://netscape/addressbook"].createInstance();
	addressbook = addressbook.QueryInterface(Components.interfaces.nsIAddressBook);

function AbDelete()
{
	dump("\AbDelete from XUL\n");
	var tree = document.getElementById('resultsTree');
	if( tree )
	{
		//get the selected elements
		var cardList = tree.getElementsByAttribute("selected", "true");
		//get the current folder
		var srcDirectory = document.getElementById('resultsTree');
		dump("srcDirectory = " + srcDirectory + "\n");
		addressbook.DeleteCards(tree, srcDirectory, cardList);
	}
}


function UpdateCardView()
{
	var selArray = document.getElementById('resultsTree').getElementsByAttribute('selected', 'true');

	if ( selArray && selArray.length == 1 )
		DisplayCardViewPane(selArray[0]);
}


function AbClose()
{
	top.window.close();
}

function AbNewAddressBook()
{
	addressbook.NewAddressBook(document.getElementById('dirTree').database, document.getElementById('resultsTree'), "My New Address Book");
}


