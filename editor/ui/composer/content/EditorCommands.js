/* Main Composer window UI control */

  /*the editor type, i.e. "text" or "html" */
  var editorType = "html";   
  /* the name of the editor. Must be unique to this window clique */
  var editorName = "EditorAppCoreHTML";
  var appCore = null;  
  var toolkitCore = null;

  function Startup()
  {
    dump("Doing Startup...\n");

    /* Get the global Editor AppCore and the XPFE toolkit core into globals here */
    appCore = XPAppCoresManager.Find(editorName);  
    dump("Looking up EditorAppCore...\n");
    if (appCore == null) {
      dump("Creating EditorAppCore...\n");
      appCore = new EditorAppCore();
      if (appCore) {
        dump("EditorAppCore has been created.\n");
				appCore.Init(editorName);
				appCore.setEditorType(editorType);
        appCore.setContentWindow( window.frames[0] );
        appCore.setWebShellWindow(window);
        appCore.setToolbarWindow(window);
        dump("EditorAppCore windows have been set.\n");
      }
    } else {
      dump("EditorAppCore has already been created! Why?\n");
    }
    toolkitCore = XPAppCoresManager.Find("ToolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("ToolkitCore");
        dump("ToolkitCore initialized for Editor\n");
      }
    } else {
      dump("ToolkitCore found\n");
    }
  }

  function EditorFind(firstTime)
  {
    if (toolkitCore && firstTime) {
      toolkitCore.ShowWindow("resource:/res/samples/find.xul", window);
    }
    
    if (appCore) {
    	appCore.find("test", true, true);
    }
  }

  function EditorShowClipboard()
  {
    dump("In EditorShowClipboard...\n");
   
    if (appCore) {
      dump("Doing EditorShowClipboard...\n");
      appCore.showClipboard(); 
    }
  }

  function SetParagraphFormat(paraFormat)
  {
    if (appCore) {
      dump("Doing SetParagraphFormat...\n");
      appCore.setParagraphFormat(paraFormat);
    }        
  }

// Debug methods to test the SELECT element used in a toolbar:  
  function OnChangeParaFormat()
  {
    dump(" *** Change Paragraph Format combobox setting\n");
  }

  function OnFocusParaFormat()
  {
    dump(" *** OnFocus -- Paragraph Format\n");
  }

  function OnBlurParaFormat()
  {
    dump(" *** OnBlur -- Paragraph Format\n");
  }

  function EditorApplyStyle(styleName)
  {
    if (appCore) {
	  	dump("Applying Style\n");
      appCore.setTextProperty(styleName, null, null);
    }
  }

  function EditorRemoveStyle(styleName)
  {
    if (appCore) {
	  	dump("Removing Style\n");
      appCore.removeTextProperty(styleName, null);
    }
  }

  function EditorGetText()
  {
    if (appCore) {
	  	dump("Getting text\n");
			var	outputText = appCore.contentsAsText;
			dump(outputText + "\n");
    }
  }

  function EditorGetHTML()
  {
    if (appCore) {
	  	dump("Getting HTML\n");
			var	outputText = appCore.contentsAsHTML;
			dump(outputText + "\n");
    }
  }

  function EditorUndo()
  {
    if (appCore) {
	    dump("Undoing\n");
      appCore.undo();
    }
  }

  function EditorRedo()
  {
    if (appCore) {
	    dump("Redoing\n");
      appCore.redo();
    }
  }

	function EditorCut()
  {
    if (appCore) {
	    dump("Cutting\n");
      appCore.cut();
    }
  }

	function EditorCopy()
  {
    if (appCore) {
	    dump("Copying\n");
      appCore.copy();
    }
  }

	function EditorPaste()
  {
    if (appCore) {
	    dump("Pasting\n");
      appCore.paste();
    }
  }

  function EditorSelectAll()
  {
    if (appCore) {
	    dump("Selecting all\n");
      appCore.selectAll();
    }
  }

	function EditorInsertText()
  {
    if (appCore) {
	    dump("Inserting text\n");
      appCore.insertText("Once more into the breach, dear friends.\n");
    }
  }

function EditorInsertLink()
  {
    dump("Starting Insert Link... appCore, toolkitCore: "+(appCore==null)+(toolkitCore==null)+"\n");
    if (appCore && toolkitCore) {
  	  dump("Link Properties Dialog starting...\n");
      toolkitCore.ShowModalDialog("chrome://editordlgs/content/EdLinkProps.xul",
          window);
    }
  }


  function EditorInsertList(listType)
  {
    if (appCore) {
      dump("Inserting link\n");
      appCore.insertList(listType);
    }
  }

  function EditorInsertImage()
  {
    dump("Starting Insert Image...\n");
    if (appCore && toolkitCore) {
      dump("Link Properties Dialog starting...\n");
      toolkitCore.ShowModalDialog("chrome://editordlgs/content/EdImageProps.xul",
          window);
    }
  }
  
  function EditorExit()
  {
    if (appCore) {
	    dump("Exiting\n");
      appCore.exit();
    }
  }

 function EditorPrintPreview() {
    if (toolkitCore) {
      toolkitCore.ShowWindow("resource:/res/samples/printsetup.html", window);
    }
  }


	function EditorTestSelection()
	{
    if (appCore)
    {
 	    dump("Testing selection\n");
	    var selection = appCore.editorSelection;
	    if (selection)
	    {
		    dump("Got selection\n");
	    	var	firstRange = selection.getRangeAt(0);
	    	if (firstRange)
	    	{
	    		dump("Range contains \"");
	    		dump(firstRange.toString() + "\"\n");
	    	}
	    }
	    
    }
	}

	function EditorTestDocument()
	{
    if (appCore)
    {
	    dump("Getting document\n");
	    var theDoc = appCore.editorDocument;
    	if (theDoc)
    	{
    		dump("Got the doc\n");
    	  dump("Document name:" + theDoc.nodeName + "\n");
    	  dump("Document type:" + theDoc.doctype + "\n");
	    }
	    else
	    {
	    		dump("Failed to get the doc\n");
	    }
    }
	}

	/* Status calls */
	function onBoldChange()
	{
		var button = document.getElementById("Editor:Style:IsBold");
		if (button)
		{
			var bold = button.getAttribute("bold");

			if ( bold == "true" ) {
				button.setAttribute( "disabled", false );
			}
			else {
				button.setAttribute( "disabled", true );
			}

		}
		else
		{
			dump("Can't find bold broadcaster!\n");
		}
	}

