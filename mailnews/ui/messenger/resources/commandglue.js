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

