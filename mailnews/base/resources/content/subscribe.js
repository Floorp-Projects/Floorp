var gSubscribeTree = null;
var gCurrentServer = null;
var okCallback = null;
var gChangeTable = {};
var gServerURI = null;

function AddItem(children,cells)
{
		var kids = document.getElementById(children);
		var item  = document.createElement("treeitem");
		var row   = document.createElement("treerow");

		for(var i = 0; i < cells.length; i++) {
			var cell  = document.createElement("treecell");
			cell.setAttribute("value", cells[i])
			cell.setAttribute("id", cells[0] + "#" + i)
			row.appendChild(cell);
		}

		item.appendChild(row);
		item.setAttribute("id",cells[0]);
		kids.appendChild(item);
}

var SubscribeDialogListener = {
	AddItem:	function(name, subscribed, count) {
		dump(name + "," + subscribed + "," + count + "\n");
		AddItem("folders",[name,subscribed,count]);
	}
};

function SetUpTree(server)
{
	dump("SetUpTree("+ server.hostname + ")\n");

	var master = server.QueryInterface(Components.interfaces.nsISubscribeDialogMaster);
	master.populateSubscribeDialog(SubscribeDialogListener);
}

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
		
		gCurrentServer.value = server.hostName;	// use gServer.prettyName?

		SetUpTree(server);

		dump("for each child of " + server.hostName + " set subscribed to true in the datasource\n");

		var folders = folder.GetSubFolders();
		
		if (folders) {
			try {
				while (true) {
					var i = folders.currentItem();
					var f = i.QueryInterface(Components.interfaces.nsIMsgFolder);
					SetState(f.name, null, 'true');
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

function SetState(uri, element, state)
{
	var subCell = element;
	dump("SetState: " + uri + "," + element + "," + state + "\n");
	
	if (!subCell) {
		subCell  = document.getElementById(uri + "#1");
		dump("subCell =" + subCell + "\n");
	}

	if (subCell) {
		subCell.setAttribute("value",state);
	}
}

function StateChanged(uri,state)
{
	dump("StateChanged(" + uri + "," + state + ")\n");
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
		
		var cells = group.getElementsByAttribute("id",uri + "#1");
		dump("cell=" + cells[0] + "\n");
		SetState(uri, cells[0], state);
		StateChanged(uri,state);
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
