function OpenURL(url)
{
	dump("\n\nOpenURL from XUL\n\n\n");
	var appCore = new MsgAppCore();
	if (appCore != null) {
	  dump("Initializing AppCore and setting Window\n");
      appCore.Init("MsgAppCore");
      appCore.SetWindow(window);
	  appCore.OpenURL(url);
   }
}

function NewMessage()
{
	dump("\n\nnewMsg from XUL\n\n\n");
	var appCore = new ComposeAppCore();
	if (appCore != null) {
		dump("Initializing ComposeAppCore and creating a new Message\n");
		appCore.Init("ComposeAppCore");
		appCore.NewMessage("resource:/res/samples/compose.xul");
	}
}

function GetNewMail()
{
	var appCore = new MsgAppCore();
	if (appCore != null) {
		dump("Initializing MsgAppCore and setting Window\n");
		appCore.Init("MsgAppCore");
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

function ChangeFolder(folderNode)
{
	var uri = folderNode.getAttribute('id');
	dump(uri);
	var tree = frames[0].frame[1].document.getElementById('threadTree');
	tree.childNodes[4].setAttribute('id', uri);
}


function ReplyMessage(tree, nodeList, msgAppCore, type)
{
  if (msgAppCore != null) {
    var appCore = new ComposeAppCore();
    if (appCore != null) {
      dump("Initializing ComposeAppCore and creating a new Message\n");
      appCore.Init("ComposeAppCore");
      appCore.ReplyMessage("resource:/res/samples/compose.xul", tree,
			   nodeList, msgAppCore, type); 
    }
  }
}

function ForwardMessage(tree, nodeList, msgAppCore, type)
{
  if (msgAppCore != null) {
    var appCore = new ComposeAppCore();
    if (appCore != null) {
      dump("Initializing ComposeAppCore and creating a new Message\n");
      appCore.Init("ComposeAppCore");
      appCore.ForwardMessage("resource:/res/samples/compose.xul", tree,
			     nodeList, msgAppCore, type); 
    }
  }
}
