var addressbook = 0;

// functions used only by addressbook

function OnLoadAddressBook()
{
	// FIX ME - later we will be able to use onload from the overlay
	OnLoadCardView();
	
	top.addressbook = Components.classes["component://netscape/addressbook"].createInstance();
	top.addressbook = top.addressbook.QueryInterface(Components.interfaces.nsIAddressBook);

	try {
		top.addressbook.SetWebShellWindow(window)
	}
	catch (ex) {
		dump("failed to set webshell window\n");
	}
}


function AbDelete()
{
	dump("\AbDelete from XUL\n");
	var tree = document.getElementById('resultsTree');
	if ( tree )
	{
		//get the selected elements
		var cardList = tree.selectedItems;
		//get the current folder
		var srcDirectory = document.getElementById('resultsTree');
		dump("srcDirectory = " + srcDirectory + "\n");
		top.addressbook.DeleteCards(tree, srcDirectory, cardList);
	}
}


function AbDeleteDirectory()
{
	dump("\AbDeleteDirectory from XUL\n");
	var tree = document.getElementById('dirTree');
	
	if ( tree && tree.selectedItems && tree.selectedItems.length )
		top.addressbook.DeleteAddressBooks(tree.database, tree, tree.selectedItems);
}


function UpdateCardView()
{
	var tree = document.getElementById('resultsTree');

	if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
		DisplayCardViewPane(tree.selectedItems[0]);
}


function AbClose()
{
	top.window.close();
}

function AbNewAddressBook()
{
	var dialog = window.openDialog("chrome://addressbook/content/abAddressBookNameDialog.xul",
								   "",
								   "chrome",
								   {title:"New Address Book",
								    okCallback:AbCreateNewAddressBook});
}

function AbCreateNewAddressBook(name)
{
	top.addressbook.NewAddressBook(document.getElementById('dirTree').database, document.getElementById('resultsTree'), name);
}

function AbPrintCard()
{
        dump("print card\n");
        try {
                addressbook.PrintCard();
        }
        catch (ex) {
                dump("failed to print card\n");
        }
}

function AbPrintAddressBook()
{
        dump("print address book \n");
        try {
                addressbook.PrintAddressbook();
        }
        catch (ex) {
                dump("failed to print address book\n");
        }
}
