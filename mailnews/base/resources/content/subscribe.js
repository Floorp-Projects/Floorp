var gSubscribeTree = null;
var gSearchTree;
var okCallback = null;
var gChangeTable = {};
var gServerURI = null;
var gSubscribableServer = null;
var gStatusBar = null;
var gNameField = null;
var gNameFieldLabel = null;
var gFolderDelimiter = ".";
var gStatusFeedback = new nsMsgStatusFeedback;
var gTimelineEnabled = false;
var gMessengerBundle = null;
var gSubscribeDeck = null;
var gSearchView = null;
var gSearchTreeBoxObject = null;
// the rdf service
var RDF = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService(Components.interfaces.nsIRDFService);
var subscribeDS = RDF.GetDataSource("rdf:subscribe");

// get the "subscribed" atom
var atomService = Components.classes["@mozilla.org/atom-service;1"].getService().QueryInterface(Components.interfaces.nsIAtomService);
var gSubscribedAtom = atomService.getAtom("subscribed").QueryInterface(Components.interfaces.nsISupports);

var gSubscribeBundle;

function goDoCommand()
{
}

function Stop()
{
    //dump("Stop()\n")
    if (gSubscribableServer) {
        gSubscribableServer.stopPopulating(msgWindow);
    }
}

function SetServerTypeSpecificTextValues()
{
    if (!gServerURI) return;

    var serverType = GetMsgFolderFromUri(gServerURI, true).server.type;

    // set the server specific ui elements
    var stringName = "foldersheaderfor-" + serverType;
    var stringval = gSubscribeBundle.getString(stringName);
    var element = document.getElementById("nameColumn");
    element.setAttribute('label',stringval);
    element = document.getElementById("nameColumn2");
    element.setAttribute('label',stringval);

    //set the delimiter
    try {
        gFolderDelimiter = gSubscribableServer.delimiter;
    }
    catch (ex) {
        //dump(ex + "\n");
        gFolderDelimiter = ".";
    }
}

function onServerClick(event)
{
	var item = event.target;
	gServerURI = item.id;
	//dump("gServerURI="+gServerURI+"\n");

	SetUpTree(false);

	SetServerTypeSpecificTextValues();
}

function SetUpServerMenu()
{
	//dump("SetUpServerMenu()\n");

    var serverMenu = document.getElementById("serverMenu");
    var menuitems = serverMenu.getElementsByAttribute("id", gServerURI);

	try {
		//dump("gServerURI="+gServerURI+"\n");
		//dump("menuitems="+menuitems+"\n");
		//dump("menuitems[0]="+menuitems[0]+"\n");
		//dump("serverMenu="+serverMenu+"\n");
    	serverMenu.selectedItem = menuitems[0];
	}
	catch (ex) {
		//dump("failed to set the selected server: " + ex + "\n");
	}

	SetServerTypeSpecificTextValues();
}

var MySubscribeListener = {
    OnDonePopulating: function() {
        gStatusFeedback._stopMeteors();

        // only re-root the tree, if it is null.
        // otherwise, we are in here because we are populating
        // a part of the tree
  
        var refValue = gSubscribeTree.getAttribute('ref');
        if (!refValue) {
            //dump("root subscribe tree at: "+ gServerURI +"\n");
            gSubscribeTree.database.AddDataSource(subscribeDS);
            gSubscribeTree.setAttribute('ref',gServerURI);
        }
	}
};

function SetUpTree(forceToServer)
{
	//dump("SetUpTree()\n");
	
	gStatusBar = document.getElementById('statusbar-icon');
	if (!gServerURI) return;

	var folder = GetMsgFolderFromUri(gServerURI, true);
	var server = folder.server;

	try {
          CleanUpSearchView();
          gSubscribableServer = server.QueryInterface(Components.interfaces.nsISubscribableServer);
          gSubscribeTree.setAttribute('ref',null);

          // enable (or disable) the search related UI
          EnableSearchUI();

          // clear out the text field when switching server
          gNameField.value = "";

          // since there is no text, switch to the non-search view...
          SwitchToNormalView();

          gSubscribeTree.database.RemoveDataSource(subscribeDS);
          gSubscribableServer.subscribeListener = MySubscribeListener;

          gStatusFeedback._startMeteors();
          gStatusFeedback.showStatusString(gSubscribeBundle.getString("pleaseWaitString"));

          gSubscribableServer.startPopulating(msgWindow, forceToServer);
	}
	catch (ex) {
          //dump("failed to populate subscribe ds: " + ex + "\n");
	}
}


function SubscribeOnUnload()
{
  try {
    CleanUpSearchView();
    gSubscribeTree.database.RemoveDataSource(subscribeDS);
  }
  catch (ex) {
    dump("failed to remove the subscribe ds: " + ex + "\n");
  }
}

function EnableSearchUI()
{
  if (gSubscribableServer.supportsSubscribeSearch) {
    gNameField.removeAttribute('disabled');
    gNameFieldLabel.removeAttribute('disabled');
  }
  else {
    gNameField.setAttribute('disabled',true);
    gNameFieldLabel.setAttribute('disabled',true);
  }
}

function SubscribeOnLoad()
{
  //dump("SubscribeOnLoad()\n");
  gSubscribeBundle = document.getElementById("bundle_subscribe");
  gMessengerBundle = document.getElementById("bundle_messenger");
	
  gSubscribeTree = document.getElementById("subscribeTree");
  gSearchTree = document.getElementById("searchTree");
  gSearchTreeBoxObject = document.getElementById("searchTree").treeBoxObject;
  gNameField = document.getElementById("namefield");
  gNameFieldLabel = document.getElementById("namefieldlabel");

  gSubscribeDeck = document.getElementById("subscribedeck");

  msgWindow = Components.classes[msgWindowContractID].createInstance(Components.interfaces.nsIMsgWindow);
  msgWindow.statusFeedback = gStatusFeedback;
  msgWindow.SetDOMWindow(window);

	// look in arguments[0] for parameters
	if (window.arguments && window.arguments[0]) {
		if ( window.arguments[0].okCallback ) {
			top.okCallback = window.arguments[0].okCallback;
		}
	}
	
	gServerURI = null;
	if (window.arguments[0].preselectedURI) {
		var uri = window.arguments[0].preselectedURI;
		//dump("subscribe: got a uri," + uri + "\n");
		var folder = GetMsgFolderFromUri(uri, true);
		//dump("folder="+folder+"\n");
		//dump("folder.server="+folder.server+"\n");
		try {
                        CleanUpSearchView();
			gSubscribableServer = folder.server.QueryInterface(Components.interfaces.nsISubscribableServer);
                        // enable (or disable) the search related UI
                        EnableSearchUI();
			gServerURI = folder.server.serverURI;
		}
		catch (ex) {
			//dump("not a subscribable server\n");
                        CleanUpSearchView();
			gSubscribableServer = null;
			gServerURI = null;
		}
	}

	if (!gServerURI) {
		//dump("subscribe: no uri\n");
		//dump("xxx todo:  use the default news server.  right now, I'm just using the first server\n");
		var serverMenu = document.getElementById("serverMenu");
		var menuitems = serverMenu.getElementsByTagName("menuitem");
		
		if (menuitems.length > 1) {
			gServerURI = menuitems[1].id;
		}
		else {
			//dump("xxx todo none of your servers are subscribable\n");
			//dump("xxx todo fix this by disabling subscribe if no subscribable server or, add a CREATE SERVER button, like in 4.x\n");
			return;
		}
	}

	SetUpServerMenu();

  SetUpTree(false);

  gNameField.focus();
}

function subscribeOK()
{
	//dump("in subscribeOK()\n")
	if (top.okCallback) {
		top.okCallback(top.gChangeTable);
	}
	Stop();
	if (gSubscribableServer) {
		gSubscribableServer.subscribeCleanup();
	}
	return true;
}

function subscribeCancel()
{
  Stop();
  if (gSubscribableServer) {
    gSubscribableServer.subscribeCleanup();
  }
  return true;
}

function SetState(name,state)
{
  var changed = gSubscribableServer.setState(name, state);
  if (changed)
    StateChanged(name,state);
}

function changeTableRecord(server, name, state) 
{
  this.server = server;
  this.name = name;
  this.state = state;
}

function StateChanged(name,state)
{
  if (gServerURI in gChangeTable) {
    if (name in gChangeTable[gServerURI]) {
      var oldValue = gChangeTable[gServerURI][name];
      if (oldValue != state)
        delete gChangeTable[gServerURI][name];
    }
    else {
      gChangeTable[gServerURI][name] = state;
    }
  }
  else {
    gChangeTable[gServerURI] = {};
    gChangeTable[gServerURI][name] = state;
  }
}

function InSearchMode()
{
    // search is the second card in the deck
    return (gSubscribeDeck.getAttribute("selectedIndex") == "1");
}

function SearchOnClick(event)
{
  // we only care about button 0 (left click) events
  if (event.button != 0 || event.originalTarget.localName != "treechildren") return;

  var row = {}, col = {}, childElt = {};
  gSearchTreeBoxObject.getCellAt(event.clientX, event.clientY, row, col, childElt);
  if (row.value == -1 || row.value > gSearchView.rowCount-1)
    return;

  if (col.value.id == "subscribedColumn2") {
    if (event.detail != 2) {
      // single clicked on the check box 
      // (in the "subscribedColumn2" column) reverse state
      // if double click, do nothing
      ReverseStateFromRow(row.value);
    }
  } else if (event.detail == 2) {
    // double clicked on a row, reverse state
    ReverseStateFromRow(row.value);
  }

  // invalidate the row
  InvalidateSearchTreeRow(row.value);
}

function ReverseStateFromRow(row)
{
    // to determine if the row is subscribed or not,
    // we get the properties for the "subscribedColumn2" cell in the row
    // and look for the "subscribed" property
    // if the "subscribed" atom is in the list of properties
    // we are subscribed
    var properties = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
    var col = gSearchTree.columns["subscribedColumn2"];
    gSearchView.getCellProperties(row, col, properties);

    var isSubscribed = (properties.GetIndexOf(gSubscribedAtom) != -1);
    SetStateFromRow(row, !isSubscribed);
}

function SetStateFromRow(row, state)
{
    var col = gSearchTree.columns["nameColumn2"];
    var name = gSearchView.getCellText(row, col);
    // we need to escape the name because
    // some news servers have newsgroups with non ASCII names
    // we need to escape those name before calling SetState()
    SetState(encodeURI(name), state);
}

function SetSubscribeState(state)
{
  try {
    // we need to iterate over the tree selection, and set the state for 
    // all rows in the selection
    var inSearchMode = InSearchMode();
    var view = inSearchMode ? gSearchView : gSubscribeTree.view;
    var colId = inSearchMode ? "nameColumn2" : "nameColumn";
    
    var sel = view.selection;
    for (var i = 0; i < sel.getRangeCount(); ++i) {
      var start = {}, end = {};
      sel.getRangeAt(i, start, end);
      for (var k = start.value; k <= end.value; ++k) {
        if (inSearchMode)
          SetStateFromRow(k, state);
        else {
          var rowRes = gSubscribeTree.builderView.getResourceAtIndex(k);
          var name = GetRDFProperty(rowRes, "Name");
          SetState(name, state);
        }
      }
    }
    
    if (inSearchMode) {
      // force a repaint
      InvalidateSearchTree();
    }
  }
  catch (ex) {
    dump("SetSubscribedState failed:  " + ex + "\n");
  }
}

function ReverseStateFromNode(row)
{
  var rowRes = gSubscribeTree.builderView.getResourceAtIndex(row);
  var isSubscribed = GetRDFProperty(rowRes, "Subscribed");
  var name = GetRDFProperty(rowRes, "Name");

  SetState(name, isSubscribed != "true");
}

function GetRDFProperty(aRes, aProp)
{
  var propRes = RDF.GetResource("http://home.netscape.com/NC-rdf#"+aProp);
  var valueRes = gSubscribeTree.database.GetTarget(aRes, propRes, true);
  return valueRes ? valueRes.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : null;
}

function SubscribeOnClick(event)
{
  // we only care about button 0 (left click) events
  if (event.button != 0 || event.originalTarget.localName != "treechildren")
   return;
 
  var row = {}, col = {}, obj = {};
  gSubscribeTree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, obj);
  if (row.value == -1 || row.value > (gSubscribeTree.view.rowCount - 1))
    return;

  if (event.detail == 2) {
    // only toggle subscribed state when double clicking something
    // that isn't a container
    if (!gSubscribeTree.view.isContainer(row.value)) {
      ReverseStateFromNode(row.value);
      return;
    } 
  }
  else if (event.detail == 1)
  {
    if (obj.value == "twisty") {
        if (gSubscribeTree.view.isContainerOpen(row.value)) {
          var uri = gSubscribeTree.builderView.getResourceAtIndex(row.value).Value;

          gStatusFeedback._startMeteors();
          gStatusFeedback.showStatusString(gSubscribeBundle.getString("pleaseWaitString"));

          gSubscribableServer.startPopulatingWithUri(msgWindow, true /* force to server */, uri);
        }
    }
    else {
      // if the user single clicks on the subscribe check box, we handle it here
      if (col.value.id == "subscribedColumn")
        ReverseStateFromNode(row.value);
    }
  }
}

function Refresh()
{
        // clear out the textfield's entry on call of Refresh()
        gNameField.value = "";
        // force it to talk to the server
        SetUpTree(true);
}

function InvalidateSearchTreeRow(row)
{
    gSearchTreeBoxObject.invalidateRow(row);
}

function InvalidateSearchTree()
{
    gSearchTreeBoxObject.invalidate();
}

function SwitchToNormalView()
{
  // the first card in the deck is the "normal" view
  gSubscribeDeck.setAttribute("selectedIndex","0");
}

function SwitchToSearchView()
{
  // the second card in the deck is the "search" view
  gSubscribeDeck.setAttribute("selectedIndex","1");
}

function Search()
{
  var searchValue = gNameField.value;
  if (searchValue.length && gSubscribableServer.supportsSubscribeSearch) {
    SwitchToSearchView();
    gSubscribableServer.setSearchValue(searchValue);

    if (!gSearchView && gSubscribableServer) {
    gSearchView = gSubscribableServer.QueryInterface(Components.interfaces.nsITreeView);
      gSearchView.selection = null;
    gSearchTreeBoxObject.view = gSearchView;
  }
  }
  else {
    SwitchToNormalView();
  }
}

function CleanUpSearchView()
{
  if (gSearchView) {
    gSearchView.selection = null;
    gSearchView = null;
  }
}

function onSearchTreeKeyPress(event)
{
  // for now, only do something on space key
  if (event.charCode != KeyEvent.DOM_VK_SPACE)
    return;

  var treeSelection = gSearchView.selection; 
  for (var i=0;i<treeSelection.getRangeCount();i++) {
    var start = {}, end = {};
    treeSelection.getRangeAt(i,start,end);
    for (var k=start.value;k<=end.value;k++)
      ReverseStateFromRow(k);

    // force a repaint
    InvalidateSearchTree();
  }
}

function onSubscribeTreeKeyPress(event)
{
  // for now, only do something on space key
  if (event.charCode != KeyEvent.DOM_VK_SPACE)
    return;

  var treeSelection = gSubscribeTree.view.selection; 
  for (var i=0;i<treeSelection.getRangeCount();i++) {
    var start = {}, end = {};
    treeSelection.getRangeAt(i,start,end);
    for (var k=start.value;k<=end.value;k++)
      ReverseStateFromNode(k);
  }
}


function doHelpButton() 
{
  openHelp("mail-subscribe");
}
