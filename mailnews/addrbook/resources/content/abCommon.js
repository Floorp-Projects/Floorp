// functions needed from abMainWindow and abSelectAddresses

// Controller object for Results Pane
var ResultsPaneController =
{
    supportsCommand: function(command)
	{
        switch ( command )
		{
			case "cmd_selectAll":
			case "cmd_delete":
			case "button_delete":
                return true;
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		//dump('ResultsPaneController::isCommandEnabled(' + command + ')\n');
		switch ( command )
		{
			case "cmd_selectAll":
				return true;
			
			case "cmd_delete":
			case "button_delete":
				var resultsTree = document.getElementById('resultsTree');
				var numSelected = 0;
				if ( resultsTree && resultsTree.selectedItems )
					numSelected = resultsTree.selectedItems.length;
				if ( command == "cmd_delete" )
				{
					if ( numSelected < 2 )
						goSetMenuValue(command, 'valueCard');
					else
						goSetMenuValue(command, 'valueCards');
				}
				return ( numSelected > 0 );
			
			default:
				return false;
		}
	},

	doCommand: function(command)
	{
		var resultsTree = document.getElementById('resultsTree');
		
		switch ( command )
		{
			case "cmd_selectAll":
				if ( resultsTree )
				{
					dump("select all now!!!!!!" + "\n");
					resultsTree.selectAll();
				}
				break;
			
			case "cmd_delete":
			case "button_delete":
				if ( resultsTree )
				{
					var cardList = resultsTree.selectedItems;
					top.addressbook.DeleteCards(resultsTree, resultsTree, cardList);
				}
				break;
		}
	},
	
	onEvent: function(event)
	{
		dump("onEvent("+event+")\n");
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
		{
			goSetMenuValue('cmd_delete', 'valueDefault');
		}
	}
};


// Controller object for Dir Pane
var DirPaneController =
{
    supportsCommand: function(command)
	{
        switch ( command )
		{
			case "cmd_selectAll":
			case "cmd_delete":
			case "button_delete":
                return true;
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		//dump('DirPaneController::isCommandEnabled(' + command + ')\n');
		switch ( command )
		{
			case "cmd_selectAll":
				return true;
			case "cmd_delete":
			case "button_delete":
				if ( command == "cmd_delete" )
					goSetMenuValue(command, 'valueAddressBook');
				var dirTree = document.getElementById('dirTree');
				if ( dirTree && dirTree.selectedItems )
					return true;
				else
					return false;

			default:
				return false;
		}
	},

	doCommand: function(command)
	{
		var dirTree = document.getElementById('dirTree');

		switch ( command )
		{
			case "cmd_selectAll":
				if ( dirTree )
				{
					dump("select all now!!!!!!" + "\n");
					dirTree.selectAll();
				}
				break;

			case "cmd_delete":
			case "button_delete":
				if ( dirTree )
					top.addressbook.DeleteAddressBooks(dirTree.database, dirTree, dirTree.selectedItems);
				break;
		}
	},
	
	onEvent: function(event)
	{
		dump("onEvent("+event+")\n");
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
		{
			goSetMenuValue('cmd_delete', 'valueDefault');
		}
	}
};

function SetupCommandUpdateHandlers()
{
	var widget;
	
	// dir pane
	widget = document.getElementById('dirTree');
	if ( widget )
		widget.controllers.appendController(DirPaneController);
	
	// results pane
	widget = document.getElementById('resultsTree');
	if ( widget )
		widget.controllers.appendController(ResultsPaneController);
}


function AbNewCard()
{
	var selectedAB = 0;
	var tree = document.getElementById('dirTree');
	if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
		selectedAB = tree.selectedItems[0].getAttribute('id');
		
	goNewCardDialog(selectedAB);
}

function AbEditCard()
{
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var resultsTree = document.getElementById('resultsTree');

	if ( resultsTree.selectedItems && resultsTree.selectedItems.length == 1 )
	{
		var uri = resultsTree.selectedItems[0].getAttribute('id');
		var card = rdf.GetResource(uri);
		card = card.QueryInterface(Components.interfaces.nsIAbCard);
		goEditCardDialog(document.getElementById('resultsTree').getAttribute('ref'),
						 card, top.gUpdateCardView);
	}
}

function AbNewMessage()
{
	var msgComposeService = Components.classes["component://netscape/messengercompose"].getService(); 
	msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService); 

	msgComposeService.OpenComposeWindowWithValues(null, 0, GetSelectedAddresses(), null, null,
												  null, null, null, null); 
}  

function GetSelectedAddresses()
{
	var item, uri, rdf, cardResource, card;
	var selectedAddresses = "";
	
	var resultsTree = document.getElementById('resultsTree');
	
	rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	if ( resultsTree.selectedItems && resultsTree.selectedItems.length )
	{
		for ( item = 0; item < resultsTree.selectedItems.length; item++ )
		{
			uri = resultsTree.selectedItems[item].getAttribute('id');
			cardResource = rdf.GetResource(uri);
			card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
			if ( selectedAddresses )
				selectedAddresses += ",";
			selectedAddresses += "\"" + card.DisplayName + "\" <" + card.PrimaryEmail + ">";
		}
	}
	return selectedAddresses;	
}


function SelectFirstAddressBook()
{
	var tree = document.getElementById('dirTree');
	var body = document.getElementById('dirTreeBody');
	if ( tree && body )
	{
		var treeitems = body.getElementsByTagName('treeitem');
		if ( treeitems && treeitems.length > 0 )
		{
			tree.selectItem(treeitems[0]);
			ChangeDirectoryByDOMNode(treeitems[0]);
		}
	}
}

function DirPaneSelectionChange()
{
	var dirTree = document.getElementById('dirTree');
	if ( dirTree && dirTree.selectedItems && (dirTree.selectedItems.length == 1) )
	{
		ChangeDirectoryByDOMNode(dirTree.selectedItems[0]);
	}
	else	
	{
		var resultsTree = document.getElementById('resultsTree');
		if ( resultsTree )
		{
			ClearResultsTreeSelection();
			resultsTree.setAttribute('ref', null);
		}
	}
}

function ChangeDirectoryByDOMNode(dirNode)
{
	var uri = dirNode.getAttribute('id');
	
	var resultsTree = document.getElementById('resultsTree');
	if ( resultsTree )
	{
		if ( uri != resultsTree.getAttribute('ref') )
		{
			ClearResultsTreeSelection();
			resultsTree.setAttribute('ref', uri);
		}
	}
}

function ResultsPaneSelectionChange()
{
	if ( top.gUpdateCardView )
		top.gUpdateCardView();
}

function ClearResultsTreeSelection()
{
	var resultsTree = document.getElementById('resultsTree');
	if ( resultsTree )
		resultsTree.clearItemSelection();
}

function GetResultsTreeChildren()
{
	var tree = document.getElementById('resultsTree');
	
	if ( tree && tree.childNodes )
	{
		for ( var index = tree.childNodes.length - 1; index >= 0; index-- )
		{
			if ( tree.childNodes[index].tagName == 'treechildren' )
			{
				return(tree.childNodes[index]);
			}
		}
	}
	return null;
}

function GetResultsTreeItem(row)
{
	var treechildren = GetResultsTreeChildren();
	
	if ( treechildren && row > 0)
	{
		var treeitems = treechildren.getElementsByTagName('treeitem');
		if ( treeitems && treeitems.length >= row )
			return treeitems[row-1];
	}
	return null;
}

function SortResultPane(column, sortKey)
{
	var node = document.getElementById(column);
	if(!node)    return(false);

	var isupports = Components.classes["component://netscape/rdf/xul-sort-service"].getService();
	if (!isupports)    return(false);
	
	var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
	if (!xulSortService)    return(false);

	// sort!!!
	sortDirection = "ascending";
    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
            sortDirection = "descending";
    else if (currentDirection == "descending")
            sortDirection = "ascending";
    else    sortDirection = "ascending";

	xulSortService.Sort(node, sortKey, sortDirection);

    return(true);
}
