var msgAppCore;
var composeAppCore;

function FindMsgAppCore()
{
  msgAppCore = XPAppCoresManager.Find("MsgAppCore");
  if (msgAppCore == null) {
    dump("FindMsgAppCore: Creating AppCore\n");
    msgAppCore = new MsgAppCore();
    dump("Initializing MsgAppCore and setting Window\n");
    msgAppCore.Init("MsgAppCore");
  }
  return msgAppCore;
}

function FindComposeAppCore()
{
  composeAppCore = XPAppCoresManager.Find("ComposeAppCore");
  if (composeAppCore == null) {
    dump("FindComposeAppCore: Creating ComposeAppCore\n");
    composeAppCore = new ComposeAppCore();
    dump("Initializing ComposeAppCore and setting Window\n");
    composeAppCore.Init("ComposeAppCore");
  }
  return composeAppCore;
}
function OpenURL(url)
{
  dump("\n\nOpenURL from XUL\n\n\n");
  var appCore = FindMsgAppCore();
  if (appCore != null) {
    appCore.SetWindow(window);
    appCore.OpenURL(url);
  }
}

function ComposeMessage(tree, nodeList, msgAppCore, type)
{
  dump("\nComposeMessage from XUL\n");
  var appCore = FindComposeAppCore();
  if (appCore != null) {
    appCore.SetWindow(window);
    appCore.NewMessage("resource:/res/mailnews/compose/msgcompose.xul", tree,
		       nodeList, msgAppCore, type); 
  }
}

function NewMessage()
{

  dump("\n\nnewMsg from XUL\n\n\n");
  ComposeMessage(null, null, null, 0);
}

function GetNewMail()
{
  var appCore = FindMsgAppCore();
  if (appCore != null) {
    appCore.SetWindow(window);
    appCore.GetNewMail();
  }
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
  dump(uri);
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
    var appCore = FindMsgAppCore();
    dump("message type ");
    dump(type);
    dump("\n");
    if (appCore && nodeList)
      appCore.SetWindow(window);
      ComposeMessage(tree, nodeList, appCore, type);
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
    rdfCore.doSort(node, sortKey, "ascending");

    return(true);


}