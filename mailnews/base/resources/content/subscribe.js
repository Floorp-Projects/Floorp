var gSubscribeTree = null;
var gCurrentServer = null;
var okCallback = null;
var gChangeTable = {};
var gServerURI = null;
var RDF = null;
var SubscribeDS = null;
var gCurrentServerURI = null;

function Stop()
{
	dump("Stop()\n");
	dump("we need to stop the news url that is running.\n");
}

function SetUpTree()
{
	dump("SetUpTree()\n");
	var nntpService = Components.classes['component://netscape/messenger/nntpservice'].getService(Components.interfaces.nsINntpService);
	nntpService = nntpService.QueryInterface(Components.interfaces.nsINntpService);
	nntpService.buildSubscribeDatasource(gCurrentServer);

	gCurrentServerURI = "news://" + gCurrentServer.hostName;
	dump("root subscribe tree at: "+gCurrentServerURI+"\n");
	gSubscribeTree.setAttribute('ref',gCurrentServerURI);
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
		gServerURI = window.arguments[0].preselectedURI;
		dump("subscribe: got a uri," + gServerURI + "\n");
		folder = GetMsgFolderFromUri(window.arguments[0].preselectedURI);
		gCurrentServer = folder.server;

		SetUpTree();

		RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
		RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
		
		SubscribeDS = RDF.GetDataSource("rdf:subscribe");
	}
	else {
		dump("subscribe: no uri\n");
	}
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

function SetState(uri, state)
{
	dump("SetState(" + uri +"," + state + ")\n");
	if (!uri || !state) return;

	try {
		var src = RDF.GetResource(uri, true);
		var prop = RDF.GetResource("http://home.netscape.com/NC-rdf#Subscribed", true);
		var oldLiteral = SubscribeDS.GetTarget(src, prop, true);
		var newLiteral = RDF.GetLiteral(state);
		dump("oldLiteral="+oldLiteral+"\n");

		if (oldLiteral) {
			dump("oldLiteral.Value="+oldLiteral.Value+"\n");
			SubscribeDS.Change(src, prop, oldLiteral, newLiteral);
		}
		else {
			SubscribeDS.Assert(src,prop, newLiteral, true);
		}
	}
	catch (ex) {
		dump("failed: " + ex + "\n");
	}
}

function StateChanged(name,state)
{
	dump("StateChanged(" + name + "," + state + ")\n");
	if (!gChangeTable[name]) {
		gChangeTable[name] = 0;
	}

	if (state == 'true') {
		gChangeTable[name] = gChangeTable[name] + 1;
	}
	else {
		gChangeTable[name] = gChangeTable[name] - 1;
	}
	dump(gChangeTable[name] + "\n");
}

function SetSubscribeState(state)
{
  dump("SetSubscribedState()\n");

  try {
	dump("subscribe button clicked\n");

	var groupList = gSubscribeTree.selectedItems;
	for (i=0;i<groupList.length;i++) {
		group = groupList[i];
		uri = group.getAttribute('id');
		dump(uri + "\n");
		name = group.getAttribute('name');
		dump(name + "\n");
		SetState(uri, state);
		StateChanged(name,state);
	}
  }
  catch (ex) {
	dump("SetSubscribedState failed:  " + ex + "\n");
  }
}

function SubscribeOnClick(event)
{
	dump("subscribe tree clicked\n");
	dump(event.target.parentNode.parentNode.getAttribute("id") + "\n");
}

function RefreshList()
{
	dump("refresh list\n");
}

