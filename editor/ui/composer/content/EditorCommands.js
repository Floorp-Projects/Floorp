/* Main Composer window UI control */

/*the editor type, i.e. "text" or "html" */
/* the name of the editor. Must be unique globally, hence the timestamp */
var editorName = "EditorAppCore." + ( new Date() ).getTime().toString();
var appCore = null;  
var toolkitCore = null;

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
  toolkitCore = XPAppCoresManager.Find("ToolkitCore");
  if (!toolkitCore)
  {
    toolkitCore = new ToolkitCore();
    if (toolkitCore) {
      toolkitCore.Init("ToolkitCore");
      dump("ToolkitCore initialized for Editor\n");
    }
  } else {
    dump("ToolkitCore found\n");
  }
}


function EditorShutdown()
{
  dump("In EditorShutdown..\n");

  appCore = XPAppCoresManager.Remove(editorName);
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

// --------------------------- Text style ---------------------------

function SetTextProperty(property, attribute, value)
{
  if (appCore) {
    appCore.setTextProperty(property, attribute, value);
  }        
}

function SetParagraphFormat(paraFormat)
{
  if (appCore) {
    appCore.setParagraphFormat(paraFormat);
  }        
}

function SetFontSize(size)
{
  if (appCore) {
    appCore.setTextProperty("font", "size", size);
  }        
}

function SetFontFace(fontFace)
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
}

function SetFontColor(color)
{
  if (appCore) {
    appCore.setTextProperty("font", "color", color);
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
  dump("Starting Insert Link... appCore, toolkitCore: "+(appCore==null)+(toolkitCore==null)+"\n");
  if (appCore && toolkitCore) {
    dump("Link Properties Dialog starting...\n");
    // go back to using this when window.opener works.
    //toolkitCore.ShowModalDialog("chrome://editordlgs/content/EdLinkProps.xul", window);
    toolkitCore.ShowWindowWithArgs("chrome://editordlgs/content/EdLinkProps.xul", window, editorName);
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
    //toolkitCore.ShowModalDialog("chrome://editordlgs/content/EdImageProps.xul", window);
    toolkitCore.ShowWindowWithArgs("chrome://editordlgs/content/EdImageProps.xul", window, editorName);
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

function CheckSpelling()
{
  if (appCore && toolkitCore) {
    dump("Check Spelling starting...\n");
    // Start the spell checker module. Return is first misspelled word
    word = appCore.startSpellChecking();
    dump(word+"\n");
    if( word == "")
    {
      dump("THERE IS NO MISSPELLED WORD!\n");
      // TODO: PUT UP A MESSAGE BOX TO TELL THE USER
      appCore.CloseSpellChecking();
    } else {
      dump("We found a MISSPELLED WORD\n");
      toolkitCore.ShowWindowWithArgs("chrome://editordlgs/content/EdSpellCheck.xul", window, editorName);
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
  core = XPAppCoresManager.Find("toolkitCore");
  if ( !core ) {
      core = new ToolkitCore();
      if ( core ) {
          core.Init("toolkitCore");
      }
  }
  if ( core ) {
      core.ShowWindowWithArgs( "chrome://editor/content/", window, url );
  } else {
      dump("Error; can't create toolkitCore\n");
  }
}


// --------------------------- Status calls ---------------------------
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
