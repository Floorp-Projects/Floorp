var addressbook = 0;

// functions used only by addressbook

function OnLoadAddressBook()
{
	// FIX ME - later we will be able to use onload from the overlay
	OnLoadCardView();
	
	top.addressbook = Components.classes["component://netscape/addressbook"].createInstance();
	top.addressbook = top.addressbook.QueryInterface(Components.interfaces.nsIAddressBook);
	top.addressbook.editCardCallback = UpdateCardView;

	try {
		top.addressbook.SetWebShellWindow(window)
	}
	catch (ex) {
		dump("failed to set webshell window\n");
	}

	// FIX ME! - can remove these as soon as waterson enables auto-registration
	document.commandDispatcher.addCommand(document.getElementById('CommandUpdate_AddressBook'));
	document.commandDispatcher.addCommand(document.getElementById('cmd_selectAll'));
	document.commandDispatcher.addCommand(document.getElementById('cmd_delete'));
	
	SetupCommandUpdateHandlers();
	SelectFirstAddressBook();
}


function CommandUpdate_AddressBook()
{
	dump("CommandUpdate_AddressBook\n");
	
	// get selection info from dir pane
	var tree = document.getElementById('dirTree');
	var oneAddressBookSelected = false;
	if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
		oneAddressBookSelected = true;
	dump("oneAddressBookSelected = " + oneAddressBookSelected + "\n");
		
	// get selection info from results pane
	var selectedAddresses = GetSelectedAddresses();
	var oneOrMoreAddressesSelected = false;
	if ( selectedAddresses )
		oneOrMoreAddressesSelected = true;
	
	// set commands to enabled / disabled
	goSetCommandEnabled('cmd_PrintCard', oneOrMoreAddressesSelected);
	goSetCommandEnabled('cmd_SortByName', oneAddressBookSelected);
	goSetCommandEnabled('cmd_SortByEmail', oneAddressBookSelected);
	goSetCommandEnabled('cmd_SortByPhone', oneAddressBookSelected);
}


/*function UpdateCommand_Delete()
{
	var command = "cmd_delete";
	
	// set enabled/disabled
	goUpdateCommand(command);
	
	var focusedElement = document.commandDispatcher.focusedElement;
	var id = focusedElement.getAttribute('id');
	var commandNode = document.getElementById(command);

	dump("focusedElement = " + focusedElement + "\n");
	dump("id = " + id + "\n");
	dump("commandNode = " + commandNode + "\n");
	if ( focusedElement && id && commandNode )
	{
		switch ( id )
		{
			case "dirTree":
				commandNode.setAttribute('value', 'Delete Address Book');
				break;
			case "resultsTree":
				commandNode.setAttribute('value', 'Delete Card');
				break;
			default:
				commandNode.setAttribute('value', 'Delete');
				break;
		}
	}
}*/


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
		top.addressbook.DeleteCards(tree, srcDirectory, cardList);
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
		top.addressbook.DeleteAddressBooks(tree.database, tree, tree.selectedItems);
}
*/


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

function AbImport()
{
	addressbook.ImportAddressBook();
}
