var cvPrefs = 0;
var addressbook = 0;
var gUpdateCardView = 0;

function OnLoadAddressBook()
{
	top.addressbook = Components.classes["component://netscape/addressbook"].createInstance();
	top.addressbook = top.addressbook.QueryInterface(Components.interfaces.nsIAddressBook);
	top.gUpdateCardView = UpdateCardView;

	InitCommonJS();
	GetCurrentPrefs();

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


function GetCurrentPrefs()
{
	// prefs
	if ( cvPrefs == 0 )
		cvPrefs = new Object;

	var prefs = Components.classes["component://netscape/preferences"];
	if ( prefs )
	{
		prefs = prefs.getService();
		if ( prefs )
			prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
	}
			
	if ( prefs )
	{
		try {
			cvPrefs.prefs = prefs;
			cvPrefs.displayLastNameFirst = prefs.GetBoolPref("mail.addr_book.displayName.lastnamefirst");
			cvPrefs.nameColumn = prefs.GetIntPref("mail.addr_book.lastnamefirst");
			cvPrefs.lastFirstSeparator = ", ";
			cvPrefs.firstLastSeparator = " ";
			cvPrefs.titlePrefix = "Card for ";
		}
		catch (ex) {
			dump("failed to get the mail.addr_book.displayName.lastnamefirst pref\n");
		}
	}
	
	// check "Show Name As" menu item based on pref
	var menuitemID;
	switch ( cvPrefs.nameColumn )
	{
		case 2:
			menuitemID = 'firstLastCmd';
			break;
		case 1:
			menuitemID = 'lastFirstCmd';
			break;
		case 0:
		default:
			menuitemID = 'displayNameCmd';
			break;
	}
	menuitem = top.document.getElementById(menuitemID);
	if ( menuitem )
		menuitem.setAttribute('checked', 'true');
}


function SetNameColumn(cmd)
{
	var prefValue;
	
	switch ( cmd )
	{
		case 'firstLastCmd':
			prefValue = 2;
			break;
		case 'lastFirstCmd':
			prefValue = 1;
			break;
		case 'displayNameCmd':
			prefValue = 0;
			break;
	}
	
	// set pref in file and locally
	cvPrefs.prefs.SetIntPref("mail.addr_book.lastnamefirst", prefValue);
	cvPrefs.nameColumn = prefValue;
	
	var selectionArray = RememberResultsTreeSelection();
	ClearResultsTreeSelection()	;
	
	RedrawResultsTree();
	
	WaitUntilDocumentIsLoaded();
	SortToPreviousSettings();
	RestoreResultsTreeSelection(selectionArray);
}


function CommandUpdate_AddressBook()
{
	goUpdateCommand('button_delete');
	
	// get selection info from dir pane
	var oneAddressBookSelected = false;
	if ( dirTree && dirTree.selectedItems && (dirTree.selectedItems.length == 1) )
		oneAddressBookSelected = true;
		
	// get selection info from results pane
	var selectedCards = GetSelectedAddresses();
	var oneOrMoreCardsSelected = false;
	if ( selectedCards )
		oneOrMoreCardsSelected = true;
	
	// set commands to enabled / disabled
	//goSetCommandEnabled('cmd_PrintCard', oneAddressBookSelected);
	goSetCommandEnabled('cmd_SortByName', oneAddressBookSelected);
	goSetCommandEnabled('cmd_SortByEmail', oneAddressBookSelected);
	goSetCommandEnabled('cmd_SortByWorkPhone', oneAddressBookSelected);
	goSetCommandEnabled('cmd_SortByOrganization', oneAddressBookSelected);
}


function UpdateCardView()
{
	if ( resultsTree && resultsTree.selectedItems && (resultsTree.selectedItems.length == 1) )
		DisplayCardViewPane(resultsTree.selectedItems[0]);
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
	top.addressbook.newAddressBook(dirTree.database, resultsTree, name);
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
	try {
		addressbook.importAddressBook();
	}
	catch (ex) {
		alert("failed to import the addressbook.\n");
		dump("import failed:  " + ex + "\n");
	}
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


