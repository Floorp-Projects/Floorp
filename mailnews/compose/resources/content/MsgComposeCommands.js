var msgComposeService = Components.classes['component://netscape/messengercompose'].getService();
msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);
var msgCompose = null;		

function GetArgs()
{
	var args = new Object();
	var data = document.getElementById("args").getAttribute("value");
	var pairs = data.split(",");
	//dump("Compose: argument: {" + data + "}\n");

	for (var i = pairs.length - 1; i >= 0; i--)
	{
		var pos = pairs[i].indexOf('=');
		if (pos == -1)
			continue;
		var argname = pairs[i].substring(0, pos);
		var argvalue = pairs[i].substring(pos + 1);
		if (argvalue.charAt(0) == "'" && argvalue.charAt(argvalue.length - 1) == "'")
			args[argname] = argvalue.substring(1, argvalue.length - 1);
		else
			args[argname] = unescape(argvalue);
	}
	return args;
}

function ComposeStartup()
{
	dump("Compose: StartUp\n");

	// Get arguments
	var args = GetArgs();
	dump("[type=" + args.type + "]\n");
	dump("[format=" + args.format + "]\n");
	dump("[originalMsg=" + args.originalMsg + "]\n");

	if (msgComposeService)
	{
		msgCompose = msgComposeService.InitCompose(window, args.originalMsg, args.type, args.format);
		if (msgCompose)
		{
			// Generate a unique number, do we have a better way?
			var date = new Date();
			sessionID = date.getTime() + Math.random();
			
			//Creating a Editor Shell
  			var editorShell = Components.classes["component://netscape/editor/editorshell"].createInstance();
  			editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
			if (!editorShell)
			{
				dump("Failed to create editorShell!\n");
				return;
			}
			
			// save the editorShell in the window. The editor JS expects to find it there.
			window.editorShell = editorShell;
			window.editorShell.Init();
			dump("Created editorShell\n");

			dump("initalizing the editor app core\n");
			SetupToolbarElements(); //defined into EditorCommands.js

			// setEditorType MUST be call before setContentWindow
			if (msgCompose.composeHTML)
			{
				window.editorShell.SetEditorType("html");
				dump("editor initialized in HTML mode\n");
			}
			else
			{
				window.editorShell.SetEditorType("text");
				window.editorShell.wrapColumn = msgCompose.wrapLength;
				dump("editor initialized in PLAIN TEXT mode\n");
			}

			window.editorShell.SetContentWindow(window.frames[0]);
			window.editorShell.SetWebShellWindow(window);
			window.editorShell.SetToolbarWindow(window);

			// Now that we have an Editor AppCore, we can finish to initialize the Compose AppCore
			msgCompose.editor = window.editorShell;
			
			//Now we are ready to load all the fields (to, cc, subject, body, etc...)
			msgCompose.LoadFields();
//			document.getElementById("msgTo").focus();
// If I call focus, Apprunner will crash later when closing this windows!
		}
	}
}

function ComposeUnload(calledFromExit)
{
	dump("\nComposeUnload from XUL\n");

	if (msgCompose && msgComposeService)
		msgComposeService.DisposeCompose(msgCompose, false);
	//...and what's about the editor appcore, how can we release it?
}

function ComposeExit()
{
	dump("\nComposeExit from XUL\n");

	//editor appcore knows how to shutdown the application, just use it...
	window.editorShell.Exit();
}

function SetDocumentCharacterSet(aCharset)
{
	dump("SetDocumentCharacterSet Callback!\n");
	dump(aCharset + "\n");

	if (msgCompose)
		msgCompose.SetDocumentCharset(aCharset);
	else
		dump("Compose has not been created!\n");
}

function SendMessage()
{
	dump("SendMessage from XUL\n");
	
	if (msgCompose != null)
		msgCompose.SendMsg(0, null);
	else
		dump("###SendMessage Error: composeAppCore is null!\n");
}
