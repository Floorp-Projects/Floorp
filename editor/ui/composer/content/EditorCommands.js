/* Main Composer window UI control */

/*the editor type, i.e. "text" or "html" */
/* the name of the editor. Must be unique globally, hence the timestamp */
var editorName = "EditorAppCore" + ( new Date() ).getTime().toString();
var appCore = null;  
var toolbar;

function EditorStartup()
{
  dump("Doing Startup...\n");

  /* Get the global Editor AppCore and the XPFE toolkit core into globals here */
  appCore = XPAppCoresManager.Find(editorName);  
  dump("Looking up EditorAppCore...\n");
  if (appCore == null)
  {
    dump("Creating EditorAppCore...\n");
    appCore = new EditorAppCore();
    if (appCore) {
      dump(editorName + " has been created.\n");
      appCore.Init(editorName);

      appCore.setWebShellWindow(window);
      appCore.setToolbarWindow(window)

      appCore.setEditorType("html");
      appCore.setContentWindow( window.frames[0] );

      // Get url for editor content
      var url = document.getElementById("args").getAttribute("value");
      // Load the source (the app core will magically know what to do).
      appCore.loadUrl(url);
      // the editor gets instantiated by the appcore when the URL has finished loading.
      dump("EditorAppCore windows have been set.\n");
    }
  } else {
    dump("EditorAppCore has already been created! Why?\n");
  }
  EditorSetup(editorName, appCore);

  // Set focus to the edit window
  window.focus();
}

function EditorSetup(p_editorName, p_appCore)
{
  editorName = p_editorName;
  appCore = p_appCore;

  // Create an object to store controls for easy access
  toolbar = new Object;
  if (!toolbar) {
    dump("Failed to create toolbar object!!!\n");
    EditorExit();
  }
  toolbar.boldButton = document.getElementById("BoldButton");
  toolbar.IsBold = document.getElementById("Editor:Style:IsBold");
}

function EditorShutdown()
{
  dump("In EditorShutdown..\n");
  // this fires at all the wrong times, so I can't do this here
  //appCore = XPAppCoresManager.Remove(appCore);
}


// --------------------------- File menu ---------------------------

function EditorNew()
{
  dump("In EditorNew..\n");
 
  appCore = XPAppCoresManager.Find(editorName);  
  if (appCore)
  {
    appCore.newWindow();
  }
}

function EditorOpen()
{
  dump("In EditorOpen..\n");
 
  appCore = XPAppCoresManager.Find(editorName);  
  if (appCore)
  {
    appCore.open();
  }
}

function EditorSave()
{
  dump("In EditorSave...\n");
 
  appCore = XPAppCoresManager.Find(editorName);  
  if (appCore)
  {
    appCore.save();
  }
}

function EditorSaveAs()
{
  dump("In EditorSave...\n");
 
  appCore = XPAppCoresManager.Find(editorName);  
  if (appCore)
  {
    appCore.saveAs();
  }
}


function EditorPrint()
{
  dump("In EditorPrint..\n");
 
  appCore = XPAppCoresManager.Find(editorName);  
  if (appCore)
  {
    appCore.print();
  }
}

function EditorClose()
{
  dump("In EditorClose...\n");
 
  appCore = XPAppCoresManager.Find(editorName);  
  if (appCore)
  {
    appCore.closeWindow();
  }
}

// --------------------------- Edit menu ---------------------------

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

function EditorFind()
{
  if (appCore) {
    appCore.find();
  }
}

function EditorFindNext()
{
  if (appCore) {
    appCore.findNext();
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

// --------------------------- Text style ---------------------------

// Currently not used???
function EditorSetTextProperty(property, attribute, value)
{
  if (appCore) {
    appCore.setTextProperty(property, attribute, value);
  }        
  dump("Set text property -- calling focus()\n");
  window.focus();
}

function EditorSetParagraphFormat(paraFormat)
{
  if (appCore) {
    appCore.setParagraphFormat(paraFormat);
  }        
  window.focus();
}

function EditorSetFontSize(size)
{
  if (appCore) {
    appCore.setTextProperty("font", "size", size);
  }        
  window.focus();
}

function EditorSetFontFace(fontFace)
{
  if (appCore) {
    if( fontFace == "tt") {
      // The old "teletype" attribute
      appCore.setTextProperty("tt", "", "");  
      // Clear existing font face
      fontFace = "";
    }
    appCore.setTextProperty("font", "face", fontFace);
  }        
  window.focus();
}

function EditorSetFontColor(color)
{
  if (appCore) {
    appCore.setTextProperty("font", "color", color);
  }        
  window.focus();
}

function EditorSetBackgroundColor(color)
{
  appCore.setBackgroundColor(color);
  window.focus();
}

function EditorApplyStyle(styleName)
{
  if (appCore) {
    dump("Applying Style\n");
    appCore.setTextProperty(styleName, null, null);
  }
  window.focus();
}

function EditorRemoveStyle(styleName)
{
  if (appCore) {
    dump("Removing Style\n");
    appCore.removeTextProperty(styleName, null);
  }
  window.focus();
}

// --------------------------- Output ---------------------------

function EditorGetText()
{
  if (appCore) {
    dump("Getting text\n");
    var  outputText = appCore.contentsAsText;
    dump(outputText + "\n");
  }
}

function EditorGetHTML()
{
  if (appCore) {
    dump("Getting HTML\n");
    var  outputText = appCore.contentsAsHTML;
    dump(outputText + "\n");
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
  dump("Starting Insert Link...\n");
  if (appCore) {
    dump("Link Properties Dialog starting...\n");
    window.openDialog("chrome://editordlgs/content/EdLinkProps.xul", "LinkDlg", "chrome", editorName);
  }
}


function EditorInsertList(listType)
{
  if (appCore) {
    dump("Inserting list\n");
    appCore.insertList(listType);
    dump("\n");
  }
}

function EditorInsertImage()
{
  if (appCore) {
    dump("Image Properties Dialog starting. Editor Name="+editorName+"\n");
    window.openDialog("chrome://editordlgs/content/EdImageProps.xul", "dlg", "chrome", editorName);
  }
}

function EditorIndent(indent)
{
  dump("indenting\n");
  if (appCore) {
    appCore.indent(indent);
  }
}

function EditorAlign(align)
{
  dump("aligning\n");
  if (appCore) {
    appCore.align(align);
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
  window.openDialog("resource:/res/samples/printsetup.html", "PrintPreview", "chrome", editorName);
}

function CheckSpelling()
{
  if (appCore) {
    dump("Check Spelling starting...\n");
    // Start the spell checker module. Return is first misspelled word
    firstMisspelledWord = appCore.startSpellChecking();
    dump(firstMisspelledWord+"\n");
    if( firstMisspelledWord == "")
    {
      dump("THERE IS NO MISSPELLED WORD!\n");
      // TODO: PUT UP A MESSAGE BOX TO TELL THE USER
      appCore.CloseSpellChecking();
    } else {
      dump("We found a MISSPELLED WORD\n");
      window.openDialog("chrome://editordlgs/content/EdSpellCheck.xul", "SpellDlg", "chrome", editorName, firstMisspelledWord);
    }
  }
}
  
// --------------------------- Debug stuff ---------------------------

function EditorTestSelection()
{
  if (appCore)
  {
    dump("Testing selection\n");
    var selection = appCore.editorSelection;
    if (selection)
    {
      dump("Got selection\n");
      var  firstRange = selection.getRangeAt(0);
      if (firstRange)
      {
        dump("Range contains \"");
        dump(firstRange.toString() + "\"\n");
      }
    }
  }
}

function EditorUnitTests()
{
  if (appCore) {
    dump("Running Unit Tests\n");
    appCore.runUnitTests();
  }
}

function EditorExit()
{
    if (appCore) {
	    dump("Exiting\n");
      appCore.exit();
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

// --------------------------- Callbacks ---------------------------
function OpenFile(url)
{
  // This is invoked from the browser app core.
  // TODO: REMOVE THIS WHEN WE STOP USING TOOLKIT CORE
  core = XPAppCoresManager.Find("toolkitCore");
  if ( !core ) {
      core = new ToolkitCore();
      if ( core ) {
          core.Init("toolkitCore");
      }
  }
  if ( core ) {
      // TODO: Convert this to use window.open() instead
      core.ShowWindowWithArgs( "chrome://editor/content/", window, url );
  } else {
      dump("Error; can't create toolkitCore\n");
  }
  window.focus();
}

// --------------------------- Status calls ---------------------------
function onBoldChange()
{
	bold = toolbar.IsBold.getAttribute("bold");
	if ( bold == "true" ) {
		toolbar.boldButton.setAttribute( "disabled", false );
	}
	else {
		toolbar.boldButton.setAttribute( "disabled", true );
	}
}

