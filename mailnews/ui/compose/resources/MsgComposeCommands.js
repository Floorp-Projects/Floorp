var editorAppCore = null;
var composeAppCore = null;

function GetArgs()
{
	var args = new Object();
	var data = document.getElementById("args").getAttribute("value");
	var pairs = data.split(",");
	dump("Compose: argument: " + data + "\n");

	for (var i = pairs.length - 1; i >= 0; i--)
	{
		var pos = pairs[i].indexOf('=');
		if (pos == -1)
			continue;
		var argname = pairs[i].substring(0, pos);
		var argvalue = pairs[i].substring(pos + 1);
		args[argname] = unescape(argvalue);
	}
	return args;
}

function ComposeStartup()
{
	dump("Compose: StartUp\n");
	var sessionID;
	var appCoreName;

	// Get arguments
	var args = GetArgs();

	// Generate a unique number, do we have a better way?
	var date = new Date();
	sessionID = date.getTime() + Math.random();

	//Creating or Getting the Compose AppCore
	if (args.name)
	{
		appCoreName = args.name;
		dump("Compose: get composeAppCore=" + args.name + "\n");
	}
	else
		appCoreName = "ComposeAppCore:" + sessionID;
	composeAppCore = XPAppCoresManager.Find(appCoreName);
	if (! composeAppCore)
	{
		dump("creating ComposeAppCore...\n");
		composeAppCore = new ComposeAppCore();
		if (composeAppCore)
		{
			composeAppCore.Init(appCoreName);
			dump("compose app core (" + sessionID + ") correctly added to app cores manager\n");
		}
	}
	if (composeAppCore)
	{
		dump("initalizing the compose app core\n");
		composeAppCore.SetWindow(window);
		composeAppCore.CompleteCallback("MessageSent();");
	}

	//Creating a Editor AppCore
	appCoreName = "EditorAppCore:" + sessionID;
	editorAppCore = XPAppCoresManager.Find(appCoreName);	
	dump("Looking up EditorAppCore...\n");
	if (! editorAppCore)
	{
		dump("Creating EditorAppCore...\n");
		editorAppCore = new EditorAppCore();
		if (editorAppCore)
		{
			editorAppCore.Init(appCoreName);
			dump("editor app core (" + sessionID + ") correctly added to app cores manager\n");
		}
	}
	else
		dump("Editor Appcore has been already created, something wrong here!!!");
	if (editorAppCore)
	{
		dump("initalizing the editor app core\n");
		editorName = appCoreName; //Very important to set this variable used by Editor js

		// setEditorType MUST be call before setContentWindow
		var useHtml;
		if (args.editorType)
		{
			switch(args.editorType.toLowerCase())
			{
				case "text": useHtml = false;		break;
				case "html": useHtml = true;		break;
				
				case "default":
				default:
					if (composeAppCore)
						useHtml = composeAppCore.useHtml;
					break;
			}
		}
		else
		{
			if (composeAppCore)
				useHtml = composeAppCore.useHtml;
		}
		
		if (useHtml)
		{
			editorAppCore.setEditorType("html");
			dump("editor initialized in HTML mode\n");
		}
		else
		{
			editorAppCore.setEditorType("text");
			dump("editor initialized in PLAIN TEXT mode\n");
		}
		editorAppCore.setContentWindow(window.frames[0]);
		editorAppCore.setWebShellWindow(window);
		editorAppCore.setToolbarWindow(window);
		if (useHtml)
			editorAppCore.loadUrl("chrome://messengercompose/content/defaultHtmlBody.html");
		else
			editorAppCore.loadUrl("chrome://messengercompose/content/defaultTextBody.html");

		// Now that we have an Editor AppCore, we can finish to initialize the Compose AppCore
		if (composeAppCore)
		{
			dump("Finish Initialization of the compose appcore\n");
			editorAppCore.wrapColumn = composeAppCore.wrapColumn;
			composeAppCore.SetEditor(editorAppCore);
			composeAppCore.useHtml = useHtml;
		}
	}
}

function ComposeUnload(calledFromExit)
{
	dump("\nPage Unloaded from XUL\n");
	//Don't use it now because it's called at the wrong time!
}

function ComposeExit()
{
	dump("\nApplication Exit from XUL\n");

	//editor appcore knows how to shutdown the application, just use it...
	if (editorAppCore)
		editorAppCore.exit();
}

function SetDocumentCharacterSet(aCharset)
{
	dump("SetDocumentCharacterSet Callback!\n");
	if (composeAppCore != null)
		composeAppCore.SetDocumentCharset(aCharset);
	else
		dump("ComposeAppCore has not been created!\n");
}

function SendMessage()
{
	dump("SendMessage from JS!\n");
	if (composeAppCore != null)
		composeAppCore.SendMessage2();
	else
		dump("###SendMessage Error: composeAppCore is null!\n");
}

function MessageSent()
{
	dump("MessageSent Callback from JS!\n");

	// Clear Them
	document.getElementById('msgTo').value = "";
	document.getElementById('msgCc').value = "";
	document.getElementById('msgBcc').value = "";
	document.getElementById('msgNewsgroup').value = "";
	document.getElementById('msgSubject').value = "";
	if (editorAppCore)
	{
		editorAppCore.selectAll();
		editorAppCore.insertText("");
	}

	window.close();	// <-- doesn't work yet!
}
