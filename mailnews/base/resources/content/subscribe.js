var gSubscribeTree = null;
var okCallback = null;
var gChangeTable = {};
var gServerURI = null;
var RDF = null;
var gSubscribeDS = null;

function Stop()
{
	dump("Stop()\n");
	dump("we need to stop the news url that is running.\n");
}

function onServerClick(event)
{
	var item = event.target;
	gServerURI = item.id;

	SetUpTree();
}

function SetUpServerMenu()
{
	dump("SetUpServerMenu()\n");

    var serverMenu = document.getElementById("serverMenu");
    var menuitems = serverMenu.getElementsByAttribute("id", gServerURI);
    
    serverMenu.selectedItem = menuitems[0];
}

var MySubscribeListener = {
	OnStopPopulating: function() {
		dump("root subscribe tree at: "+ gServerURI +"\n");
		gSubscribeTree.setAttribute('ref',gServerURI);
	}
};

function SetUpTree()
{
	dump("SetUpTree()\n");
	
	var folder = GetMsgFolderFromUri(gServerURI);
	var server = folder.server;

	try {
		subscribableServer = server.QueryInterface(Components.interfaces.nsISubscribableServer);

		subscribableServer.subscribeListener = MySubscribeListener;

		subscribableServer.populateSubscribeDatasource(null /* eventually, a nsIMsgWindow */);
	}
	catch (ex) {
		dump("failed to populate subscribe ds: " + ex + "\n");
	}
}

function SubscribeOnLoad()
{
	dump("SubscribeOnLoad()\n");
	
   	gSubscribeTree = document.getElementById('subscribetree');

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
	
	if (window.arguments[0].preselectedURI) {
		var uri = window.arguments[0].preselectedURI;
		dump("subscribe: got a uri," + uri + "\n");
		folder = GetMsgFolderFromUri(uri);
		gServerURI = folder.server.serverURI;
	}
	else {
		dump("subscribe: no uri\n");
		var serverMenu = document.getElementById("serverMenu");
		var menuitems = serverMenu.getElementsByTagName("menuitem");
		gServerURI = menuitems[1].id;
	}

	SetUpServerMenu();
	SetUpTree();

	RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
		
	gSubscribeDS = RDF.GetDataSource("rdf:subscribe");
}

function subscribeOK()
{
	dump("in subscribeOK()\n")
	if (top.okCallback) {
		top.okCallback(top.gServerURI,top.gChangeTable);
	}
	return true;
}

function subscribeCancel()
{
	dump("in subscribeCancel()\n");
	Stop();
	return true;
}

function SetState(uri,name,state,stateStr)
{
	dump("SetState(" + uri +"," + name + "," + state + "," + stateStr + ")\n");
	if (!uri || !stateStr) return;

	try {
		var src = RDF.GetResource(uri, true);
		var prop = RDF.GetResource("http://home.netscape.com/NC-rdf#Subscribed", true);
		var oldLiteral = gSubscribeDS.GetTarget(src, prop, true);
		dump("oldLiteral="+oldLiteral+"\n");

		oldValue = oldLiteral.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
		dump("oldLiteral.Value="+oldValue+"\n");
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
	dump("StateChanged(" + name + "," + state + ")\n");
	dump("start val=" +gChangeTable[name] + "\n");

	if (gChangeTable[name] == undefined) {
		dump(name+" not in table yet\n");
		gChangeTable[name] = state;
	}
	else {
		var oldValue = gChangeTable[name];
		dump(name+" already in table\n");
		if (oldValue != state) {
			gChangeTable[name] = undefined;
		}
	}

	dump("end val=" +gChangeTable[name] + "\n");
}

function SetSubscribeState(state)
{
  dump("SetSubscribedState()\n");
 
  var stateStr;
  if (state) {
  	stateStr = 'true';
  }
  else {
	stateStr = 'false';
  }

  try {
	dump("subscribe button clicked\n");
	
	var groupList = gSubscribeTree.selectedItems;
	for (i=0;i<groupList.length;i++) {
		group = groupList[i];
		uri = group.getAttribute('id');
		dump(uri + "\n");
		name = group.getAttribute('name');
		dump(name + "\n");
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
	dump("ReverseState("+uri+")\n");
}

function SubscribeOnClick(event)
{
	if (event.clickCount == 2) {
		ReverseStateFromNode(event.target.parentNode.parentNode);
	}
}

function RefreshList()
{
	dump("refresh list\n");
}

