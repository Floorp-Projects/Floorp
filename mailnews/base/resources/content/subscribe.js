var gSubscribeTree = null;
var okCallback = null;
var gChangeTable = {};
var gServerURI = null;
var gSubscribableServer = null;
var gStatusBar = null;
var gNameField = null;
var gNameFieldLabel = null;
var gFolderDelimiter = ".";
var gStatusFeedback = new nsMsgStatusFeedback;
var gSubscribeDeck = null;
var gSearchView = null;
var gSearchOutlinerBoxObject = null;
// the rdf service
var RDF = '@mozilla.org/rdf/rdf-service;1'
RDF = Components.classes[RDF].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
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

    var serverType = GetMsgFolderFromUri(gServerURI).server.type;
	
    // set the server specific ui elements
    var stringName = "foldersheaderfor-" + serverType;
    var stringval = gSubscribeBundle.getString(stringName);
    var element = document.getElementById("foldersheaderlabel");
    element.setAttribute('label',stringval);
    element = document.getElementById("nameCol");
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
		gStatusFeedback.showProgress(0);
		gStatusFeedback.showStatusString(gSubscribeBundle.getString("doneString"));
		gStatusBar.setAttribute("mode","normal");

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

	var folder = GetMsgFolderFromUri(gServerURI);
	var server = folder.server;

	try {
          CleanUpSearchView();
          gSubscribableServer = server.QueryInterface(Components.interfaces.nsISubscribableServer);
          gSubscribeTree.setAttribute('ref',null);

          // enable (or disable) the search related UI
          EnableSearchUI();

          // clear out the text field when switching server
          gNameField.setAttribute('value',"");

          // since there is no text, switch to the non-search view...
          SwitchToNormalView();

          gSubscribeTree.database.RemoveDataSource(subscribeDS);
          gSubscribableServer.subscribeListener = MySubscribeListener;

          gStatusFeedback.showProgress(0);
          gStatusFeedback.showStatusString(gSubscribeBundle.getString("pleaseWaitString"));
          gStatusBar.setAttribute("mode","undetermined");

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
	
  gSubscribeTree = document.getElementById('subscribetree');
  gSearchOutlinerBoxObject = document.getElementById("searchOutliner").boxObject.QueryInterface(Components.interfaces.nsIOutlinerBoxObject);
  gNameField = document.getElementById('namefield');
  gNameFieldLabel = document.getElementById('namefieldlabel');
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
		var folder = GetMsgFolderFromUri(uri);
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
  if (changed) {
    StateChanged(name,state);
  }
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
  if (event.button != 0) return;

  var t = event.originalTarget;

  if (t.localName == "outlinerbody") {
    var row = new Object;
    var colID = new Object;
    var childElt = new Object;

    gSearchOutlinerBoxObject.getCellAt(event.clientX, event.clientY, row, colID, childElt);

    // if they are clicking on empty rows, drop the event 
    if (row.value + 1 > gSearchView.rowCount) return;

    if (colID.value == "subscribedCol") {
      if (event.detail != 2) {
        // single clicked on the check box 
        // (in the "subscribedCol" column) reverse state
        // if double click, do nothing
        ReverseStateFromRow(row.value);
      }
    }
    else if (event.detail == 2) {
      // double clicked on a row, reverse state
      ReverseStateFromRow(row.value);
    }

    // invalidate the row
    InvalidateSearchOutlinerRow(row.value);
  }
}

function ReverseStateFromRow(row)
{
    // to determine if the row is subscribed or not,
    // we get the properties for the "subscribedCol" cell in the row
    // and look for the "subscribed" property
    // if the "subscribed" atom is in the list of properties
    // we are subscribed
    var properties = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
    gSearchView.getCellProperties(row, "subscribedCol", properties);

    var isSubscribed = (properties.GetIndexOf(gSubscribedAtom) != -1);
   
    var name = gSearchView.getCellText(row,"nameCol");
    // we need to escape the name because
    // some news servers have newsgroups with non ASCII names
    // we need to escape those name before calling SetState()
    SetState(escape(name), !isSubscribed);
}

function SetSubscribeState(state)
    
{   var i, name;
  try {
    if (InSearchMode()) {
        // if we are in "search" mode, we need to iterate over the
        // outliner selection, and set the state for all elements
        // in the selection
        
        var outlinerSelection = gSearchView.selection; 
        for (i=0;i<outlinerSelection.getRangeCount();i++) {
          var start = new Object;
          var end = new Object;
          outlinerSelection.getRangeAt(i,start,end);
          for (var k=start.value;k<=end.value;k++) {
            name = gSearchView.getCellText(k,"nameCol");
            // we need to escape the name because
            // some news servers have newsgroups with non ASCII names
            // we need to escape those name before calling SetState()
            SetState(escape(name),state);
          }
        }
        // force a repaint of the outliner since states have changed
        InvalidateSearchOutliner();
    }
    else {
      // we are in the "normal" mode
      var groupList = gSubscribeTree.selectedItems;
      for (i=0;i<groupList.length;i++) {
        var group = groupList[i];
        name = group.getAttribute('name');
        SetState(name,state);
      }
    }
  }
  catch (ex) {
    //dump("SetSubscribedState failed:  " + ex + "\n");
  }
}

function ReverseStateFromNode(node)
{
  var state;

  if (node.getAttribute('Subscribed') == "true") {
    state = false;
  }
  else {
    state = true;
  }
 
  var uri = node.getAttribute('id');
  var name = node.getAttribute('name');
  SetState(name, state);
}


function SubscribeOnClick(event)
{
  // we only care about button 0 (left click) events
  if (event.button != 0) return;
 
  var t = event.originalTarget;

	if (event.detail == 2) {
		ReverseStateFromNode(t.parentNode.parentNode);
	}
	else {
 		if (t.getAttribute('twisty') == 'true') {
        	var treeitem = t.parentNode.parentNode.parentNode;
			var open = treeitem.getAttribute('open');
			if(open == "true") {
				var uri = treeitem.getAttribute("id");	
				
				//dump("do twisty for " + uri + "\n");

				gStatusFeedback.showProgress(0);
				gStatusFeedback.showStatusString(gSubscribeBundle.getString("pleaseWaitString"));
				gStatusBar.setAttribute("mode","undetermined");

				gSubscribableServer.startPopulatingWithUri(msgWindow, true /* force to server */, uri);
			}
		}
		else {
            // if the user clicks on the subscribe check box, we handle it here
            if (t.localName == "image") {
                ReverseStateFromNode(t.parentNode.parentNode.parentNode);
                return;
            }

		}
	}
}

function Refresh()
{
	// force it to talk to the server
	SetUpTree(true);
}

function InvalidateSearchOutlinerRow(row)
{
    gSearchOutlinerBoxObject.invalidateRow(row);
}

function InvalidateSearchOutliner()
{
    gSearchOutlinerBoxObject.invalidate();
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
    gSearchView = gSubscribableServer.QueryInterface(Components.interfaces.nsIOutlinerView);
      gSearchView.selection = null;
    gSearchOutlinerBoxObject.view = gSearchView;
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
