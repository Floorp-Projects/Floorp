var messenger = Components.classes['component://netscape/messenger'].createInstance();
messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);

var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

//put this in a function so we can change the position in hierarchy if we have to.
function GetFolderTree()
{
	var folderTree = frames[0].frames[0].document.getElementById('folderTree'); 
	return folderTree;
}

function FindMessenger()
{
  return messenger;
}

function OpenURL(url)
{
  dump("\n\nOpenURL from XUL\n\n\n");
  messenger.OpenURL(url);
}

function ComposeMessage(tree, nodeList, messenger, type)
{
	dump("\nComposeMessage from XUL\n");

	// Generate a unique number, do we have a better way?
	var date = new Date();
	sessionID = date.getTime() + Math.random();
	
	var composeAppCoreName = "ComposeAppCore:" + sessionID;
	var composeAppCore = XPAppCoresManager.Find(composeAppCoreName);
	if (! composeAppCore)
	{
		composeAppCore = new ComposeAppCore();
		if (composeAppCore)
		{
			composeAppCore.Init(composeAppCoreName);
			//argument:
			//	name=<name of the appcore>
			//	editorType=[default | html | text]			; default means use the prefs value send_html
			var args = "name=" + composeAppCoreName + ",editorType=default";
			composeAppCore.NewMessage("chrome://messengercompose/content/", args, tree, nodeList, type);
			dump("Created a compose appcore from Messenger, " + args);
		}
	}
}

function NewMessage()
{

  dump("\n\nnewMsg from XUL\n\n\n");
  ComposeMessage(null, null, null, 0);
}

function GetNewMessages()
{
	var folderTree = frames[0].frames[0].document.getElementById('folderTree'); 
	var selectedFolderList = folderTree.getElementsByAttribute("selected", "true");
	if(selectedFolderList.length > 0)
	{
		var selectedFolder = selectedFolderList[0];
		messenger.GetNewMessages(folderTree.database, selectedFolder);
	}
	else {
		dump("Nothing was selected\n");
	}
}

function MsgAccountManager()
{
    dump('Opening account manager..\n');
    messenger.AccountManager(window);
}

function MsgSubscribe()
{
    dump('open subscribe window.\n');
}

function LoadMessage(messageNode)
{
  var uri = messageNode.getAttribute('id');
  dump(uri);
  OpenURL(uri);
}

function ChangeFolderByDOMNode(folderNode)
{
  var uri = folderNode.getAttribute('id');
  dump(uri + "\n");
  ChangeFolderByURI(uri);
}

function ChangeFolderByURI(uri)
{
  var tree = frames[0].frames[1].document.getElementById('threadTree');
  tree.childNodes[5].setAttribute('id', uri);
}

function ComposeMessageWithType(type)
{
  dump("\nMsgReplyMessage from XUL\n");
  var tree = frames[0].frames[1].document.getElementById('threadTree');
  if(tree) {
    dump("tree is valid\n");
    var nodeList = tree.getElementsByAttribute("selected", "true");
    dump("message type ");
    dump(type);
    dump("\n");
    ComposeMessage(tree, nodeList, messenger, type);
  }
}

function SortThreadPane(column, sortKey)
{
	var node = frames[0].frames[1].document.getElementById(column);
	if(!node)
		return false;

	var rdfCore = XPAppCoresManager.Find("RDFCore");
	if (!rdfCore)
	{
		rdfCore = new RDFCore();
		if (!rdfCore)
		{
			return(false);
		}

		rdfCore.Init("RDFCore");

	}

	// sort!!!
	sortDirection = "ascending";
    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
            sortDirection = "descending";
    else if (currentDirection == "descending")
            sortDirection = "ascending";
    else    sortDirection = "ascending";

    rdfCore.doSort(node, sortKey, sortDirection);

    return(true);


}

function MsgPreferences()
{
  var prefsCore = XPAppCoresManager.Find("PrefsCore");
  if (!prefsCore) {
    prefsCore = new PrefsCore();
    if (prefsCore) {
      prefsCore.Init("PrefsCore");
    }
  }
  if (prefsCore) {
    prefsCore.ShowWindow(window);
  }
}


function GetSelectedFolderResource()
{
	var folderTree = GetFolderTree();
	var selectedFolderList = folderTree.getElementsByAttribute("selected", "true");
	var selectedFolder = selectedFolderList[0];
	var uri = selectedFolder.getAttribute('id');


	var folderResource = RDF.GetResource(uri);
	return folderResource;

}

function SetFolderCharset(folderResource, aCharset)
{
	var folderTree = GetFolderTree();

	var db = folderTree.database;
	var db2 = db.QueryInterface(Components.interfaces.nsIRDFDataSource);

	var charsetResource = RDF.GetLiteral(aCharset);
	var charsetProperty = RDF.GetResource("http://home.netscape.com/NC-rdf#Charset");

	db2.Assert(folderResource, charsetProperty, charsetResource, true);
}
