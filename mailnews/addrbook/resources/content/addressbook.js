var addressbook = 0;
var gUpdateCardView = 0;

function OnLoadAddressBook()
{

	top.addressbook = Components.classes["component://netscape/addressbook"].createInstance();
	top.addressbook = top.addressbook.QueryInterface(Components.interfaces.nsIAddressBook);
	top.gUpdateCardView = UpdateCardView;

	// FIX ME - later we will be able to use onload from the overlay
	OnLoadCardView();
	
	try {
		top.addressbook.setWebShellWindow(window)
	}
	catch (ex) {
		dump("failed to set webshell window\n");
	}

	SetupCommandUpdateHandlers();
	SelectFirstAddressBook();
}


function CommandUpdate_AddressBook()
{
	goUpdateCommand('button_delete');
	
	// get selection info from dir pane
	var tree = document.getElementById('dirTree');
	var oneAddressBookSelected = false;
	if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
		oneAddressBookSelected = true;
		
	// get selection info from results pane
	var selectedCards = GetSelectedAddresses();
	var oneOrMoreCardsSelected = false;
	if ( selectedCards )
		oneOrMoreCardsSelected = true;
	
	// set commands to enabled / disabled
	goSetCommandEnabled('cmd_PrintCard', oneOrMoreCardsSelected);
	goSetCommandEnabled('cmd_SortByName', oneAddressBookSelected);
	goSetCommandEnabled('cmd_SortByEmail', oneAddressBookSelected);
	goSetCommandEnabled('cmd_SortByPhone', oneAddressBookSelected);
}


function UpdateCardView()
{
	var tree = document.getElementById('resultsTree');

	if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
		DisplayCardViewPane(tree.selectedItems[0]);
	else
		ClearCardViewPane();
}


function AbClose()
{
	top.close();
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
	top.addressbook.newAddressBook(document.getElementById('dirTree').database, document.getElementById('resultsTree'), name);
}

function AbPrintCard()
{
        dump("print card\n");
        try {
                addressbook.printCard();
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

function AbImport()
{
	addressbook.importAddressBook();
}

/*
function AbDelete()
{
//	dump("\AbDelete from XUL\n");
	var tree = document.getElementById('resultsTree');
	if ( tree )
	{
		//get the selected elements
		var cardList = tree.selectedItems;
		//get the current folder
		var srcDirectory = document.getElementById('resultsTree');
		dump("srcDirectory = " + srcDirectory + "\n");
		top.addressbook.deleteCards(tree, srcDirectory, cardList);
	}
}
*/

/*
function AbDeleteDirectory()
{
//	dump("\AbDeleteDirectory from XUL\n");
	var tree = document.getElementById('dirTree');
	
//	if ( tree && tree.selectedItems && tree.selectedItems.length )
	if ( tree )
		top.addressbook.deleteAddressBooks(tree.database, tree, tree.selectedItems);
}
*/


