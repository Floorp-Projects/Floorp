var gSubscribeTree = null;
var okCallback = null;
var gChangeTable = {};
var gServerURI = null;
var gSubscribableServer = null;
var gStatusBar = null;
var gNameField = null;
var gFolderDelimiter = ".";
var gStatusFeedback = new nsMsgStatusFeedback;

// the rdf service
var RDF = '@mozilla.org/rdf/rdf-service;1'
RDF = Components.classes[RDF].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
var subscribeDS = RDF.GetDataSource("rdf:subscribe");

// used for caching the tree children (in typedown)
var lastTreeChildrenValue = null;
var lastTreeChildren = null;

function Stop()
{
	//dump("Stop()\n")
    gSubscribableServer.stopPopulating(msgWindow);
}

var Bundle = srGetStrBundle("chrome://messenger/locale/subscribe.properties");

function SetServerTypeSpecificTextValues()
{
	if (!gServerURI) return;

	var serverType = GetMsgFolderFromUri(gServerURI).server.type;
	
	//set the server specific ui elements

    var element = document.getElementById("foldertextlabel");
	var stringName = "foldertextfor-" + serverType;
	var stringval = Bundle.GetStringFromName(stringName);
	element.setAttribute('value',stringval);

	stringName = "foldersheaderfor-" + serverType;
	stringval = Bundle.GetStringFromName(stringName);
    element = document.getElementById("foldersheaderlabel");
	element.setAttribute('value',stringval);

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
		gStatusFeedback.ShowProgress(100);
		gStatusFeedback.ShowStatusString(Bundle.GetStringFromName("doneString"));
		gStatusBar.setAttribute("mode","normal");

        // only re-root the tree, if it is null.
        // otherwise, we are in here because we are populating
        // a part of the tree
  
        var refValue = gSubscribeTree.getAttribute('ref');
        //dump("ref = " + refValue + refValue.length + "\n");
        if (refValue == "null") {
            //dump("root subscribe tree at: "+ gServerURI +"\n");
            gSubscribeTree.database.AddDataSource(subscribeDS);
            gSubscribeTree.setAttribute('ref',gServerURI);
        }
	}
};

function SetUpTree(forceToServer)
{
	//dump("SetUpTree()\n");
	
	// forget the cached tree children
	lastTreeChildrenValue = null;
	lastTreeChildren = null;

	gStatusBar = document.getElementById('statusbar-icon');
	if (!gServerURI) return;


	var folder = GetMsgFolderFromUri(gServerURI);
	var server = folder.server;

	try {
		gSubscribableServer = server.QueryInterface(Components.interfaces.nsISubscribableServer);
	    gSubscribeTree.setAttribute('ref',null);

        // clear out the text field when switching server
		gNameField.setAttribute('value',"");

        gSubscribeTree.database.RemoveDataSource(subscribeDS);
		gSubscribableServer.subscribeListener = MySubscribeListener;

		gStatusFeedback.ShowProgress(0);
		gStatusFeedback.ShowStatusString(Bundle.GetStringFromName("pleaseWaitString"));
		gStatusBar.setAttribute("mode","undetermined");

		gSubscribableServer.startPopulating(msgWindow, forceToServer);
	}
	catch (ex) {
		//dump("failed to populate subscribe ds: " + ex + "\n");
	}
}


function SubscribeOnLoad()
{
	//dump("SubscribeOnLoad()\n");
	
    gSubscribeTree = document.getElementById('subscribetree');
	gNameField = document.getElementById('namefield');

	msgWindow = Components.classes[msgWindowContractID].createInstance(Components.interfaces.nsIMsgWindow);
    msgWindow.statusFeedback = gStatusFeedback;
	msgWindow.SetDOMWindow(window);

	doSetOKCancel(subscribeOK,subscribeCancel);

	// look in arguments[0] for parameters
	if (window.arguments && window.arguments[0]) {
		if ( window.arguments[0].title ) {
			top.window.title = window.arguments[0].title;
		}
		
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
			gSubscribableServer = folder.server.QueryInterface(Components.interfaces.nsISubscribableServer);
			gServerURI = folder.server.serverURI;
		}
		catch (ex) {
			//dump("not a subscribable server\n");
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
	//dump("in subscribeCancel()\n");
	Stop();
	if (gSubscribableServer) {
		gSubscribableServer.subscribeCleanup();
	}
	return true;
}

function SetState(name,state)
{
	//dump("SetState(" + name + "," + state + ")\n");

    var changed = gSubscribableServer.setState(name, state);
	if (changed) {
        StateChanged(name,state);
	}
}

function changeTableRecord(server, name, state) {
    this.server = server;
    this.name = name;
    this.state = state;
}

function StateChanged(name,state)
{
	//dump("StateChanged(" + name + "," + state + ")\n");
	if (gChangeTable[gServerURI] == undefined) {
        gChangeTable[gServerURI] = {};
		gChangeTable[gServerURI][name] = state;
	}
	else {
        if (gChangeTable[gServerURI][name] == undefined) {
            gChangeTable[gServerURI][name] = state;
        }
        else {
		    var oldValue = gChangeTable[gServerURI,name];
		    if (oldValue != state) {
			    gChangeTable[gServerURI][name] = undefined;
		    }
        }
	}
}

function SetSubscribeState(state)
{
  //dump("SetSubscribedState()\n");
 
  try {
	//dump("subscribe button clicked\n");
	
	var groupList = gSubscribeTree.selectedItems;
	for (i=0;i<groupList.length;i++) {
		group = groupList[i];
		uri = group.getAttribute('id');
		//dump(uri + "\n");
		name = group.getAttribute('name');
		//dump(name + "\n");
		SetState(name,state);
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
	var	name = node.getAttribute('name');
	SetState(name, state);
}

function SubscribeOnClick(event)
{
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

				gStatusFeedback.ShowProgress(0);
				gStatusFeedback.ShowStatusString(Bundle.GetStringFromName("pleaseWaitString"));
				gStatusBar.setAttribute("mode","undetermined");

				gSubscribableServer.startPopulatingWithUri(msgWindow, true /* force to server */, uri);
			}
		}
		else {
			var name = t.parentNode.parentNode.getAttribute('name');
			if (name && (name.length > 0)) {
				gNameField.setAttribute('value',name);
			}
		}
	}
}

function Refresh()
{
	// force it to talk to the server
	SetUpTree(true);
}

function trackGroupInTree()
{
  var portion = gNameField.value;
  selectNodeByName( portion );  
}


function selectNodeByName( aMatchString )
{
  var lastDot = aMatchString.lastIndexOf(gFolderDelimiter);
  var nodeValue = lastDot != -1 ? aMatchString.substring(0, lastDot) : aMatchString;
  
  var chain = aMatchString.split(gFolderDelimiter);
  var node;
  if( chain.length == 1 ) {
	if (lastTreeChildrenValue != "") {
    	node = getTreechildren(gSubscribeTree);
    	if( !node ) return;
		lastTreeChildrenValue = "";
		lastTreeChildren = node;
		//dump("cache miss!\n");
	}
	else {
		node = lastTreeChildren;
		//dump("cache hit!\n");
	}
  }
  else {
	// if we can, use the cached tree children
	if (nodeValue != lastTreeChildrenValue) {
    	node = gSubscribeTree.getElementsByAttribute("name",nodeValue)[0];

		// expand the node, if we need to
    	if( node.getAttribute("container") == "true" && 
        	node.getAttribute("open") != "true" ) {
      		node.setAttribute("open","true");
		}
    	node = getTreechildren(node);
    	//dump("*** node = " + node.localName + "\n");

		lastTreeChildren = node;
		lastTreeChildrenValue = nodeValue;
		//dump("cache miss!\n");
	}
	else {
		//dump("cache hit!\n");
		node = lastTreeChildren;
	}
  }
  
  // find the match, using a binary search.
  var totalItems = node.childNodes.length;
  if (totalItems == 0) return;
  var lastLow = 0;
  var lastHigh = totalItems;
  while (true) {
  	var i = Math.floor((lastHigh + lastLow) / 2);
	//dump(i+","+lastLow+","+lastHigh+"\n");
	var currItem = node.childNodes[i];
	var currValue = (currItem.getAttribute("name")).substring(0,aMatchString.length);
	//dump(currValue+" vs "+aMatchString+"\n");
	if (currValue > aMatchString) {
		if (lastHigh == i) return;
		lastHigh = i;
    }
	else if (currValue < aMatchString) {
		if (lastLow == i) return;
		lastLow = i;
	}
	else {
      SelectFirstMatch(lastLow, i, node.childNodes, aMatchString);
      return;
	}
  }
}

function SelectFirstMatch(startNode, endNode, nodes, matchStr)
{
  // this code is to make sure we select the alphabetically first match
  // not just any match (see bug #60242)

  // find the match, using a binary search.

  var lastMatch = nodes[endNode];
  var totalItems = endNode - startNode;
  if (totalItems == 0) {
    gSubscribeTree.selectItem( lastMatch );
    gSubscribeTree.ensureElementIsVisible( lastMatch );
    return;
  }

  var lastLow = startNode;
  var lastHigh = endNode;
  while (true) {
    var i = Math.floor((lastHigh + lastLow) / 2);
    //dump(i+","+lastLow+","+lastHigh+"\n");
    var currItem = nodes[i];
    var currValue = (currItem.getAttribute("name")).substring(0,matchStr.length);
    //dump(currValue+" vs "+matchStr+"\n");
    if (currValue > matchStr) {
        gSubscribeTree.selectItem( lastMatch );
        gSubscribeTree.ensureElementIsVisible( lastMatch );
        return;
    }
    else if (currValue < matchStr) {
        if (lastLow == i) {
            gSubscribeTree.selectItem( lastMatch );
            gSubscribeTree.ensureElementIsVisible( lastMatch );
            return;
        }
        lastLow = i;
    }
    else {
        lastMatch = currItem;
        if (lastHigh == i) {
            gSubscribeTree.selectItem( lastMatch );
            gSubscribeTree.ensureElementIsVisible( lastMatch );
            return;
        }
        lastHigh = i;
    }
  }
}

function getTreechildren( aTreeNode )
{
  if( aTreeNode.childNodes ) 
    {
      for( var i = 0; i < aTreeNode.childNodes.length; i++ )
      {
        if( aTreeNode.childNodes[i].localName.toLowerCase() == "treechildren" )
          return aTreeNode.childNodes[i];
      }
      return aTreeNode;
    }
  return null;
}
