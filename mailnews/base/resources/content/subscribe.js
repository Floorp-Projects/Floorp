var gSubscribeTree = null;
var gCurrentServer = null;
var okCallback = null;
var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);
var datasource = rdf.GetDataSource('rdf:newshostinfo');
var gChangeTable = {};
var gServerURI = null;

function SubscribeOnLoad()
{
	dump("SubscribeOnLoad()\n");

    gSubscribeTree = document.getElementById('subscribetree');
    gCurrentServer = document.getElementById('currentserver');

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
	
	// pre select the folderPicker, based on what they selected in the folder pane
	if (window.arguments[0].preselectedURI) {
		gServerURI = window.arguments[0].preselectedURI;
		var folder = GetMsgFolderFromUri(window.arguments[0].preselectedURI);
		var server = folder.server;
		
		gSubscribeTree.setAttribute('ref','urn:' + server.hostName);
		gCurrentServer.value = server.hostName;	// use gServer.prettyName?

		dump("for each child of news://" + server.hostName + " set subscribed to true in the datasource\n");

		var folders = folder.GetSubFolders();
		
		if (folders) {
			try {
				while (true) {
					var i = folders.currentItem();
					var f = i.QueryInterface(Components.interfaces.nsIMsgFolder);
					dump(f.URI+ "\n");
					dump(f.name + "\n");

					dump('urn:' + f.name + "\n");
					SetState('urn:' + f.name, 'true');
					folders.next();
				}
			}
			catch (ex) {
				dump("no more subfolders\n");
			}
		}
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
	dump("in subscribeCancel()\n")
	return true;
}

function SetState(uri, state)
{
	var group = rdf.GetResource(uri);
	dump(group + "\n");

    var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#subscribed", true);
	var target = datasource.GetTarget(group, prop, true);
	dump(target + "\n");
	if (target) {
		var targetValue = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
		//dump(targetValue + "\n");
        if (targetValue) {
			targetValue = targetValue.Value;
			dump(targetValue + "\n");
			if (targetValue) {
				var newLiteral = rdf.GetLiteral(state);
				var newTarget = newLiteral.QueryInterface(Components.interfaces.nsIRDFNode);
				datasource.Change(group,prop,target,newTarget);
			}
		}
	}
}

function StateChanged(uri,state)
{
	if (!gChangeTable[uri]) {
		gChangeTable[uri] = 0;
	}

	if (state == 'true') {
		gChangeTable[uri] = gChangeTable[uri] + 1;
	}
	else {
		gChangeTable[uri] = gChangeTable[uri] - 1;
	}
	dump(gChangeTable[uri] + "\n");
}

function SetSubscribeState(state)
{
	dump("subscribe button clicked\n");

	var groupList = gSubscribeTree.selectedItems;
	for (i=0;i<groupList.length;i++) {
		group = groupList[i];
		uri = group.getAttribute('id');
		dump(uri + "\n");
		SetState(uri, state);
		StateChanged(uri,state);
	}
}

function SubscribeOnClick(event)
{
	dump("subscribe tree clicked\n");
	dump(event.target.parentNode.parentNode.getAttribute("id") + "\n");
}
