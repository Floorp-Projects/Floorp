var gSubscribeTree = null;
var okCallback = null;
var gChangeTable = {};
var gServerURI = null;
var gSubscribableServer = null;
var RDF = null;
var gSubscribeDS = null;
var gStatusBar = null;
var gNameField = null;
var gFolderDelimiter = ".";
var gStatusFeedback = new nsMsgStatusFeedback;

// used for caching the tree children (in typedown)
var lastTreeChildrenValue = null;
var lastTreeChildren = null;


function SetUpRDF()
{
	if (!RDF) {
			RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
			RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
	}
		
	if (!gSubscribeDS) {
		gSubscribeDS = RDF.GetDataSource("rdf:subscribe");
	}
}

function Stop()
{
	dump("xxx todo implement stop.\n");
}

var Bundle = srGetStrBundle("chrome://messenger/locale/subscribe.properties");

function SetServerTypeSpecificTextValues()
{
	if (!gServerURI) return;

	var serverType = GetMsgFolderFromUri(gServerURI).server.type;
	//dump("serverType="+serverType+"\n");
	
	/* set the server specific ui elements */
    var element = document.getElementById("foldertextlabel");
	var stringName = "foldertextfor-" + serverType;
	var stringval = Bundle.GetStringFromName(stringName);
	element.setAttribute('value',stringval);

	stringName = "foldersheaderfor-" + serverType;
	stringval = Bundle.GetStringFromName(stringName);
    element = document.getElementById("foldersheaderlabel");
	element.setAttribute('value',stringval);

	// xxx todo, fix this hack
	// qi the server to get a nsISubscribable server
	// and ask it for the delimiter
	if (serverType == "nntp") {
		gFolderDelimiter = ".";
	}
	else {
		gFolderDelimiter = "/";
	}
}

function onServerClick(event)
{
	var item = event.target;
	gServerURI = item.id;
	//dump("gServerURI="+gServerURI+"\n");

	SetServerTypeSpecificTextValues();
	SetUpTree(false);
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
		dump("failed to set the selected server: " + ex + "\n");
	}

	SetServerTypeSpecificTextValues();
}

var MySubscribeListener = {
	OnStopPopulating: function() {
		// only re-root the tree, if it is null.
		// otherwise, we are in here because we are populating
		// a part of the tree

		var refValue = gSubscribeTree.getAttribute('ref');
		//dump("ref = " + refValue + refValue.length + "\n");
		if (refValue == "null") {
			dump("root subscribe tree at: "+ gServerURI +"\n");
			gSubscribeTree.setAttribute('ref',gServerURI);
		}

		gStatusFeedback.ShowProgress(100);
		gStatusFeedback.ShowStatusString(Bundle.GetStringFromName("doneString"));
		gStatusBar.setAttribute("mode","normal");
	}
};

function SetUpTree(forceToServer)
{
	//dump("SetUpTree()\n");
	
	// forget the cached tree children
	lastTreeChildrenValue = null;
	lastTreeChildren = null;

	SetUpRDF();
	
	gStatusBar = document.getElementById('statusbar-icon');
	gSubscribeTree.setAttribute('ref',null);

	if (!gServerURI) return;

	var folder = GetMsgFolderFromUri(gServerURI);
	var server = folder.server;

	try {
		gSubscribableServer = server.QueryInterface(Components.interfaces.nsISubscribableServer);

		gSubscribableServer.subscribeListener = MySubscribeListener;

		gStatusFeedback.ShowProgress(0);
		gStatusFeedback.ShowStatusString(Bundle.GetStringFromName("pleaseWaitString"));
		gStatusBar.setAttribute("mode","undetermined");

		gSubscribableServer.populateSubscribeDatasource(msgWindow, forceToServer);
	}
	catch (ex) {
		dump("failed to populate subscribe ds: " + ex + "\n");
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
			dump("not a subscribable server\n");
			gSubscribableServer = null;
			gServerURI = null;
		}
	}

	if (!gServerURI) {
		//dump("subscribe: no uri\n");
		dump("xxx todo:  use the default news server.  right now, I'm just using the first server\n");
		var serverMenu = document.getElementById("serverMenu");
		var menuitems = serverMenu.getElementsByTagName("menuitem");
		
		if (menuitems.length > 1) {
			gServerURI = menuitems[1].id;
		}
		else {
			dump("xxx todo none of your servers are subscribable\n");
			dump("xxx todo fix this by disabling subscribe if no subscribable server or, add a CREATE SERVER button, like in 4.x\n");
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
		top.okCallback(top.gServerURI,top.gChangeTable);
	}
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

function SetState(uri,name,state,stateStr)
{
	//dump("SetState(" + uri +"," + name + "," + state + "," + stateStr + ")\n");
	if (!uri || !stateStr) return;

	try {
		// xxx should we move this code into nsSubscribableServer.cpp?
		var src = RDF.GetResource(uri, true);
		var prop = RDF.GetResource("http://home.netscape.com/NC-rdf#Subscribed", true);
		var oldLiteral = gSubscribeDS.GetTarget(src, prop, true);
		//dump("oldLiteral="+oldLiteral+"\n");

		oldValue = oldLiteral.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
		//dump("oldLiteral.Value="+oldValue+"\n");
		if (oldValue != stateStr) {
			var newLiteral = RDF.GetLiteral(stateStr);
			gSubscribeDS.Change(src, prop, oldLiteral, newLiteral);
			StateChanged(name,state);
		}
	}
	catch (ex) {
		dump("failed: " + ex + "\n");
	}
}

function StateChanged(name,state)
{
	//dump("StateChanged(" + name + "," + state + ")\n");
	//dump("start val=" +gChangeTable[name] + "\n");

	if (gChangeTable[name] == undefined) {
		//dump(name+" not in table yet\n");
		gChangeTable[name] = state;
	}
	else {
		var oldValue = gChangeTable[name];
		//dump(name+" already in table\n");
		if (oldValue != state) {
			gChangeTable[name] = undefined;
		}
	}

	//dump("end val=" +gChangeTable[name] + "\n");
}

function SetSubscribeState(state)
{
  //dump("SetSubscribedState()\n");
 
  var stateStr;
  if (state) {
  	stateStr = 'true';
  }
  else {
	stateStr = 'false';
  }

  try {
	//dump("subscribe button clicked\n");
	
	var groupList = gSubscribeTree.selectedItems;
	for (i=0;i<groupList.length;i++) {
		group = groupList[i];
		uri = group.getAttribute('id');
		//dump(uri + "\n");
		name = group.getAttribute('name');
		//dump(name + "\n");
		SetState(uri,name,state,stateStr);
	}
  }
  catch (ex) {
	dump("SetSubscribedState failed:  " + ex + "\n");
  }
}

function ReverseStateFromNode(node)
{
	var stateStr;
	var state;

	if (node.getAttribute('Subscribed') == "true") {
		stateStr = "false";
		state = false;
	}
	else {
		stateStr = "true";
		state = true;
	}
	
	var uri = node.getAttribute('id');
	var	name = node.getAttribute('name');
	SetState(uri, name, state, stateStr);
}

function ReverseState(uri)
{
	//dump("ReverseState("+uri+")\n");
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
				
				dump("do twisty for " + uri + "\n");

				gStatusFeedback.ShowProgress(0);
				gStatusFeedback.ShowStatusString(Bundle.GetStringFromName("pleaseWaitString"));
				gStatusBar.setAttribute("mode","undetermined");

				gSubscribableServer.populateSubscribeDatasourceWithUri(msgWindow, true /* force to server */, uri);
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

function RefreshList()
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
      gSubscribeTree.selectItem( currItem );
      gSubscribeTree.ensureElementIsVisible( currItem );
	  return;
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
