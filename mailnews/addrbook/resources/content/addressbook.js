var cvPrefs = 0;
var addressbook = 0;
var gUpdateCardView = 0;
var gAddressBookBundle;

var gTotalCardsElement = null;

// Constants that correspond to choices
// in Address Book->View -->Show Name as
const kDisplayName = 0;
const kLastNameFirst = 1;
const kFirstNameFirst = 2;

var addressBookObserver = {
  onAssert: function(aDataSource, aSource, aProperty, aTarget)
  {
    UpdateAddDeleteCounts();
  },

  onUnassert: function(aDataSource, aSource, aProperty, aTarget)
  {
    UpdateAddDeleteCounts();
  },

  onChange: function(aDataSource, aSource, aProperty, aOldTarget, aNewTarget)
  { },

  onMove: function(aDataSource,
                aOldSource,
                aNewSource,
                aProperty,
                aTarget)
  {},

  beginUpdateBatch: function(aDataSource)
  {},

  endUpdateBatch: function(aDataSource)
  {}
}

function OnUnloadAddressBook()
{
	try
	{
		var dataSource = top.rdf.GetDataSource("rdf:addressdirectory");
		dataSource.RemoveObserver (addressBookObserver);
	}
	catch (ex)
	{
		dump(ex + "\n");
		dump("Error removing from RDF datasource\n");
	}
}

function OnLoadAddressBook()
{
	gAddressBookBundle = document.getElementById("bundle_addressBook");
	verifyAccounts(null); 	// this will do migration, if we need to.

	top.addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance();
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

	try
	{
		var dataSource = top.rdf.GetDataSource("rdf:addressdirectory");
		dataSource.AddObserver (addressBookObserver);
	}
	catch (ex)
	{
		dump(ex + "\n");
		dump("Error adding to RDF datasource\n");
	}
}


function GetCurrentPrefs()
{
	// prefs
	if ( cvPrefs == 0 )
		cvPrefs = new Object;

	var prefs = Components.classes["@mozilla.org/preferences-service;1"];
	if ( prefs )
	{
		prefs = prefs.getService();
		if ( prefs )
			prefs = prefs.QueryInterface(Components.interfaces.nsIPrefBranch);
	}
			
	if ( prefs )
	{
		try {
			cvPrefs.prefs = prefs;
			cvPrefs.displayLastNameFirst = prefs.getBoolPref("mail.addr_book.displayName.lastnamefirst");
			cvPrefs.nameColumn = prefs.getIntPref("mail.addr_book.lastnamefirst");
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
		case kFirstNameFirst:
			menuitemID = 'firstLastCmd';
			break;
		case kLastNameFirst:
			menuitemID = 'lastFirstCmd';
			break;
		case kDisplayName:
		default:
			menuitemID = 'displayNameCmd';
			break;
	}
	var menuitem = top.document.getElementById(menuitemID);
	if ( menuitem )
		menuitem.setAttribute('checked', 'true');
}


function SetNameColumn(cmd)
{
	var prefValue;
	
	switch ( cmd )
	{
		case 'firstLastCmd':
			prefValue = kFirstNameFirst;
			break;
		case 'lastFirstCmd':
			prefValue = kLastNameFirst;
			break;
		case 'displayNameCmd':
			prefValue = kDisplayName;
			break;
	}
	
	// set pref in file and locally
	cvPrefs.prefs.setIntPref("mail.addr_book.lastnamefirst", prefValue);
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
	var prefsAttr = new Array;
	var prefsValue = new Array;
	prefsAttr[0]  = "description";
	prefsValue[0]  = name;  
	top.addressbook.newAddressBook(dirTree.database, resultsTree, 1, prefsAttr, prefsValue);
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
	statusFeedback = Components.classes["@mozilla.org/messenger/statusfeedback;1"].createInstance();
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
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);

    var selArray = dirTree.selectedItems;
    var count = selArray.length;
    debugDump("selArray.length = " + count + "\n");
    if (!count)
        return;

    var isPersonalOrCollectedAbsSelectedForDeletion = false;
    var parentArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
    if (!parentArray) 
      return; 

    for ( var i = 0; i < count; ++i )
    {
        debugDump("    nodeId #" + i + " = " + selArray[i].getAttribute("id") + "\n");
		
        // check to see if personal or collected address books is selected for deletion.
        // if yes, prompt the user an appropriate message saying these cannot be deleted
        // if no, mark the selected items for deletion
        if ((selArray[i].getAttribute("id") != "moz-abmdbdirectory://history.mab") &&
             (selArray[i].getAttribute("id") != "moz-abmdbdirectory://abook.mab"))
        {
            var parent = selArray[i].parentNode.parentNode;
            if (parent)
            {
                var parentId;
                if (parent == dirTree)
                    parentId = "moz-abdirectory://";
                else	
                    parentId = parent.getAttribute("id");

                debugDump("    parentId #" + i + " = " + parentId + "\n");
                var dirResource = rdf.GetResource(parentId);
                var parentDir = dirResource.QueryInterface(Components.interfaces.nsIAbDirectory);
                parentArray.AppendElement(parentDir);
            }
        }
        else 
        {
            if (promptService)
            {
                promptService.alert(window,
                    gAddressBookBundle.getString("cannotDeleteTitle"), 
                    gAddressBookBundle.getString("cannotDeleteMessage"));
            }

            isPersonalOrCollectedAbsSelectedForDeletion = true;
            break;
        }
    }

    if (!isPersonalOrCollectedAbsSelectedForDeletion) {
        var confirmDeleteAddressbook =
            gAddressBookBundle.getString("confirmDeleteAddressbook");

        if(!window.confirm(confirmDeleteAddressbook))
            return;

        top.addressbook.deleteAddressBooks(dirTree.database, parentArray, dirTree.selectedItems);
    }
}

function clickResultsTree(event)
{
    // we only care about button 0 (left click) events
    if (event.button != 0) return;

    if (event.target.localName != "treecell" &&
        event.target.localName != "treeitem")
        return;

    if ( event.detail == 2 ) top.AbEditCard();
}

function UpdateAddDeleteCounts()
{
  if ( dirTree && dirTree.selectedItems && (dirTree.selectedItems.length == 1) )
  {
    var dirUri = dirTree.selectedItems[0].getAttribute("id");
    UpdateStatusCardCounts(dirUri);
  }
}

function GetTotalCardCountElement()
{
  if(gTotalCardsElement) return gTotalCardsElement;
  var totalCardCountElement = document.getElementById('statusText');
  gTotalCardsElement = totalCardCountElement;
  return totalCardCountElement;
}

function UpdateStatusCardCounts(uri)
{
  if (top.addressbook)
  {
    try
    {
      var dirResource = rdf.GetResource(uri);
      var parentDir = dirResource.QueryInterface(Components.interfaces.nsIAbDirectory);
      if (parentDir && !parentDir.isMailList)
      {
        try
        {
          var totalCards = parentDir.getTotalCards(false);
          SetTotalCardStatus(parentDir.dirName, totalCards);
        }
        catch(ex)
        {
          dump("Fail to get card total from "+uri+"\n");
        }
      }
    }
    catch(ex)
    {
      dump("Fail to get directory from "+uri+"\n");
    }
  }
}

function SetTotalCardStatus(name, total)
{
  var totalElement = GetTotalCardCountElement();
  if (totalElement)
  {
    try 
    {
      var numTotal = gAddressBookBundle.getFormattedString("totalCardStatus", [name, total]);   
      totalElement.setAttribute("label", numTotal);
    }
    catch(ex)
    {
      dump("Fail to set total cards in status\n");
    }
  }
  else
      dump("Can't find status bar\n");
}
