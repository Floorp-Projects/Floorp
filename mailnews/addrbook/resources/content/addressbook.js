var cvPrefs = 0;
var addressbook = 0;
var gUpdateCardView = 0;

function OnLoadAddressBook()
{
	verifyAccounts(); 	// this will do migration, if we need to.

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

	//workaround - add setTimeout to make sure dynamic overlays get loaded first
	setTimeout('SelectFirstAddressBook()',0);
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
	var dialog = window.openDialog("chrome://messenger/content/addressbook/abAddressBookNameDialog.xul",
								   "",
								   "chrome,titlebar",
								   {okCallback:AbCreateNewAddressBook});
}

function AbCreateNewAddressBook(name)
{
	top.addressbook.newAddressBook(dirTree.database, resultsTree, name);
}

function AbPrintCard()
{
	dump("print card\n");

	var selectedItems = resultsTree.selectedItems;
	var numSelected = selectedItems.length;

	if (numSelected == 0)
	{
		dump("AbPrintCard(): No card selected.\n");
		return false;
	}  
	var statusFeedback;
	statusFeedback = Components.classes["component://netscape/messenger/statusfeedback"].createInstance();
	statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

	var selectionArray = new Array(numSelected);

	var totalCard = 0;
	for(var i = 0; i < numSelected; i++)
	{
		var uri = selectedItems[i].getAttribute("id");
		var cardResource = top.rdf.GetResource(uri);
		var card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
		if (card.printCardUrl.length)
		{
			selectionArray[totalCard++] = card.printCardUrl;
			dump("printCardUrl = " + card.printCardUrl + "\n");
		}
	}

	printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul",
										"",
										"chrome,dialog=no,all",
										totalCard, selectionArray, statusFeedback);

	return true;
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


function AbDeleteDirectory()
{
	dump("\AbDeleteDirectory from XUL\n");

    var commonDialogsService 
        = Components.classes["component://netscape/appshell/commonDialogs"].getService();
    commonDialogsService 
        = commonDialogsService.QueryInterface(Components.interfaces.nsICommonDialogs);

    var selArray = dirTree.selectedItems;
    var count = selArray.length;
    debugDump("selArray.length = " + count + "\n");
    if (count == 0)
        return;

    var isPersonalOrCollectedAbsSelectedForDeletion = false;
    var parentArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
    if ( !parentArray ) return (false); 

    for ( var i = 0; i < count; ++i )
    {
        debugDump("    nodeId #" + i + " = " + selArray[i].getAttribute("id") + "\n");
		
        // check to see if personal or collected address books is selected for deletion.
        // if yes, prompt the user an appropriate message saying these cannot be deleted
        // if no, mark the selected items for deletion
        if ((selArray[i].getAttribute("id") != "abdirectory://history.mab") &&
             (selArray[i].getAttribute("id") != "abdirectory://abook.mab"))
        {
            var parent = selArray[i].parentNode.parentNode;
            if (parent)
            {
                if (parent == dirTree)
                    var parentId = "abdirectory://";
                else	
                    var parentId = parent.getAttribute("id");

                debugDump("    parentId #" + i + " = " + parentId + "\n");
                var dirResource = rdf.GetResource(parentId);
                var parentDir = dirResource.QueryInterface(Components.interfaces.nsIAbDirectory);
                parentArray.AppendElement(parentDir);
            }
        }
        else 
        {
            if (commonDialogsService)
            {
                commonDialogsService.Alert(window,
                    Bundle.GetStringFromName("cannotDeleteTitle"), 
                    Bundle.GetStringFromName("cannotDeleteMessage"));
            }

            isPersonalOrCollectedAbsSelectedForDeletion = true;
            break;
        }
    }

    if (!isPersonalOrCollectedAbsSelectedForDeletion) {
        var confirmDeleteAddressbook =
            Bundle.GetStringFromName("confirmDeleteAddressbook");

        if(!window.confirm(confirmDeleteAddressbook))
            return;

        top.addressbook.deleteAddressBooks(dirTree.database, parentArray, dirTree.selectedItems);
    }
}

function clickResultsTree(event)
{
    if (event.target.localName != "treecell" &&
        event.target.localName != "treeitem")
        return;

	if ( event.detail == 2 ) top.AbEditCard();
}

