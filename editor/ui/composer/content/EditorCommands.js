/* Main Composer window UI control */

var toolbar;

function EditorStartup(editorType)
{
  dump("Doing Startup...\n");
  contentWindow = window.frames[0];

  dump("Trying to make an editor appcore through the component manager...\n");

  var editorShell = Components.classes["component://netscape/editor/editorshell"].createInstance();
  editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
  if (!editorShell)
  {
    dump("Failed to create editor shell\n");
    window.close();
    return;
  }
  
  // store the editor shell in the window, so that child windows can get to it.
  window.editorShell = editorShell;
  
  window.editorShell.Init();
  window.editorShell.SetWebShellWindow(window);
  window.editorShell.SetToolbarWindow(window)
  window.editorShell.SetEditorType(editorType);
  window.editorShell.SetContentWindow(contentWindow);

  // Get url for editor content and load it.
  // the editor gets instantiated by the editor shell when the URL has finished loading.
  var url = document.getElementById("args").getAttribute("value");
  window.editorShell.LoadUrl(url);
  
  dump("EditorAppCore windows have been set.\n");
  SetupToolbarElements();

  // Set focus to the edit window
  // (A bug currently prevents this from working,
  //  the actual edit window is a child of the webshell window 
  //  designated as the contentWindow)
  contentWindow.focus();
}

function SetupToolbarElements()
{
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
  //editorShell = XPAppCoresManager.Remove(editorShell);
}


// --------------------------- File menu ---------------------------

function EditorNew()
{
  dump("In EditorNew..\n");
  window.editorShell.NewWindow();
}

function EditorOpen()
{
  dump("In EditorOpen..\n");
  window.editorShell.Open();
}

function EditorNewPlaintext()
{
  dump("In EditorNewPlaintext..\n");
 
  core = XPAppCoresManager.Find("toolkitCore");
  if ( !core ) {
    core = new ToolkitCore();
    if ( core ) {
      core.Init("toolkitCore");
    }
  }
  if ( core ) {
    core.ShowWindowWithArgs( "chrome://editor/content/TextEditorAppShell.xul", window, "chrome://editor/content/EditorInitPagePlain.html" );
  } else {
    dump("Error; can't create toolkitCore\n");
  }
}

function EditorNewBrowser()
{
  dump("In EditorNewPlaintext..\n");
 
  core = XPAppCoresManager.Find("toolkitCore");
  if ( !core ) {
    core = new ToolkitCore();
    if ( core ) {
      core.Init("toolkitCore");
    }
  }
  if ( core ) {
    core.ShowWindowWithArgs( "chrome://navigator/", window, "" );
  } else {
    dump("Error; can't create toolkitCore\n");
  }
}

function EditorSave()
{
  dump("In EditorSave...\n");
  window.editorShell.Save();
}

function EditorSaveAs()
{
  dump("In EditorSave...\n");
  window.editorShell.SaveAs();
}


function EditorPrint()
{
  dump("In EditorPrint..\n");
  window.editorShell.Print();
}

function EditorClose()
{
  dump("In EditorClose...\n");
  window.editorShell.CloseWindow();
}

// --------------------------- Edit menu ---------------------------

function EditorUndo()
{
  dump("Undoing\n");
  window.editorShell.Undo();
}

function EditorRedo()
{
  dump("Redoing\n");
  window.editorShell.Redo();
}

function EditorCut()
{
  window.editorShell.Cut();
}

function EditorCopy()
{
  window.editorShell.Copy();
}

function EditorPaste()
{
  window.editorShell.Paste();
}

function EditorPasteAsQuotation()
{
  window.editorShell.PasteAsQuotation();
}

function EditorPasteAsQuotationCited(citeString)
{
  window.editorShell.PasteAsCitedQuotation(CiteString);
}

function EditorSelectAll()
{
  window.editorShell.SelectAll();
}

function EditorFind()
{
  window.editorShell.Find();
}

function EditorFindNext()
{
  window.editorShell.FindNext();
}

function EditorShowClipboard()
{
  dump("In EditorShowClipboard...\n");
  window.editorShell.ShowClipboard(); 
}

// --------------------------- View menu ---------------------------

function EditorSetDocumentCharacterSet(aCharset)
{
  dump(aCharset);
  dump(" NOT IMPLEMENTED YET\n");
}


// --------------------------- Text style ---------------------------

function EditorSetTextProperty(property, attribute, value)
{
  window.editorShell.SetTextProperty(property, attribute, value);
  dump("Set text property -- calling focus()\n");
  contentWindow.focus();
}

function EditorSetParagraphFormat(paraFormat)
{
  window.editorShell.paragraphFormat = paraFormat;
  contentWindow.focus();
}

function EditorSetFontSize(size)
{
  if( size == "0" || size == "normal" || 
      size == "+0" )
  {
    window.editorShell.RemoveTextProperty("font", size);
    dump("Removing font size\n");
  } else {
    dump("Setting font size\n");
    window.editorShell.SetTextProperty("font", "size", size);
  }
  contentWindow.focus();
}

function EditorSetFontFace(fontFace)
{
  if( fontFace == "tt") {
    // The old "teletype" attribute
    window.editorShell.SetTextProperty("tt", "", "");  
    // Clear existing font face
    fontFace = "";
    window.editorShell.SetTextProperty("font", "face", fontFace);
  }        
  contentWindow.focus();
}

function EditorSetFontColor(color)
{
  window.editorShell.SetTextProperty("font", "color", color);
  contentWindow.focus();
}

function EditorSetBackgroundColor(color)
{
  window.editorShell.SetBackgroundColor(color);
  contentWindow.focus();
}

function EditorApplyStyle(styleName)
{
  dump("applying style\n");
  window.editorShell.SetTextProperty(styleName, "", "");
  contentWindow.focus();
}

function EditorRemoveStyle(styleName)
{
  window.editorShell.RemoveTextProperty(styleName, "");
  contentWindow.focus();
}

function EditorRemoveLinks()
{
  dump("NOT IMPLEMENTED YET\n");
}

// --------------------------- Output ---------------------------

function EditorGetText()
{
  if (window.editorShell) {
    dump("Getting text\n");
    var  outputText = window.editorShell.contentsAsText;
    dump(outputText + "\n");
  }
}

function EditorGetHTML()
{
  if (window.editorShell) {
    dump("Getting HTML\n");
    var  outputText = window.editorShell.contentsAsHTML;
    dump(outputText + "\n");
  }
}

function EditorInsertText()
{
  if (window.editorShell) {
    dump("Inserting text\n");
    window.editorShell.InsertText("Once more into the breach, dear friends.\n");
  }
}

function EditorInsertLink()
{
   if (window.editorShell) {
    window.openDialog("chrome://editordlgs/content/EdLinkProps.xul", "LinkDlg", "chrome", "");
  }
  contentWindow.focus();
}

function EditorInsertImage()
{
  if (window.editorShell) {
    window.openDialog("chrome://editordlgs/content/EdImageProps.xul", "dlg", "chrome", "");
  }
  contentWindow.focus();
}

function EditorInsertHLine()
{
  if (window.editorShell) {

    // Inserting an HLine is different in that we don't use properties dialog
    //  unless we are editing an existing line's attributes
    //  We get the last-used attributes from the prefs and insert immediately

    tagName = "hr";
    hLine = window.editorShell.GetSelectedElement(tagName);

    if (hLine) {
      // We only open the dialog for an existing HRule
      window.openDialog("chrome://editordlgs/content/EdHLineProps.xul", "dlg", "chrome", "");
    } else {
      hLine = window.editorShell.CreateElementWithDefaults(tagName);
      if (hLine) {
        window.editorShell.InsertElement(hLine, false);
      }
    }
  }
  contentWindow.focus();
}

function EditorInsertNamedAnchor()
{
  if (window.editorShell) {
    window.openDialog("chrome://editordlgs/content/EdNamedAnchorProps.xul", "dlg", "chrome", "");
  }
  contentWindow.focus();
}

function EditorIndent(indent)
{
  dump("indenting\n");
  window.editorShell.Indent(indent);
  contentWindow.focus();
}

function EditorInsertList(listType)
{
  dump("Inserting list\n");
  window.editorShell.InsertList(listType);
}

function EditorInsertImage()
{
  if (window.editorShell) {
    dump("Image Properties Dialog starting.\n");
    window.openDialog("chrome://editordlgs/content/EdImageProps.xul", "dlg", "chrome", "");
  }
}

function EditorAlign(align)
{
  dump("aligning\n");
  window.editorShell.Align(align);
}

function EditorPrintPreview()
{
  window.openDialog("resource:/res/samples/printsetup.html", "PrintPreview", "chrome", "");
}

function CheckSpelling()
{
  var spellChecker = window.editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
  if (spellChecker)
  {
    dump("Check Spelling starting...\n");
    // Start the spell checker module. Return is first misspelled word
    firstMisspelledWord = spellChecker.StartSpellChecking();
    dump(firstMisspelledWord+"\n");
    if( firstMisspelledWord == "")
    {
      dump("THERE IS NO MISSPELLED WORD!\n");
      // TODO: PUT UP A MESSAGE BOX TO TELL THE USER
      window.editorShell.CloseSpellChecking();
    } else {
      dump("We found a MISSPELLED WORD\n");
      window.openDialog("chrome://editordlgs/content/EdSpellCheck.xul", "SpellDlg", "chrome", "", firstMisspelledWord);
    }
  }
}
function OnCreateAlignmentPopup()
{
  dump("Creating Alignment popup window\n");
}
  
// --------------------------- Debug stuff ---------------------------

function EditorExecuteScript(fileSpec)
{
  fileSpec.openStreamForReading();

  var buf         = { value:null };
  var tmpBuf      = { value:null };
  var didTruncate = { value:false };

  // Log files can be quite huge, so read in a line
  // at a time and execute it:

  while (!fileSpec.eof())
  {
    buf.value         = "";
    didTruncate.value = true;

    // Keep looping until we get a complete line of
    // text, or we hit the end of file:

    while (didTruncate.value && !fileSpec.eof())
    {
      didTruncate.value = false;
      fileSpec.readLine(tmpBuf, 1024, didTruncate);
      buf.value += tmpBuf.value;

      // XXX Need to null out tmpBuf.value to avoid crashing
      // XXX in some JavaScript string allocation method.
      // XXX This is probably leaking the buffer allocated
      // XXX by the readLine() implementation.

      tmpBuf.value = null;
    }

    eval(buf.value);
  }

  buf.value = null;
}

function EditorGetScriptFileSpec()
{
  var fs = Components.classes["component://netscape/filespec"].createInstance();
  fs = fs.QueryInterface(Components.interfaces.nsIFileSpec);
  fs.UnixStyleFilePath = "journal.js";
  return fs;
}

function EditorStartLog()
{
  var fs;

  if (window.editorShell)
  {
    fs = EditorGetScriptFileSpec();
    window.editorShell.StartLogging(fs);

    fs = null;
  }
}

function EditorStopLog()
{
  if (window.editorShell)
  {
    window.editorShell.StopLogging();
  }
}

function EditorRunLog()
{
  var fs;
  fs = EditorGetScriptFileSpec();
  EditorExecuteScript(fs);
}

function EditorGetNodeFromOffsets(offsets)
{
  var node = null;
  var i;

  node = window.editorShell.editorDocument;

  for (i = 0; i < offsets.length; i++)
  {
    node = node.childNodes[offsets[i]];
  }

  return node;
}

function EditorSetSelectionFromOffsets(selRanges)
{
  var rangeArr, start, end, i, node, offset;
  var selection = window.editorShell.editorSelection;

  selection.clearSelection();

  for (i = 0; i < selRanges.length; i++)
  {
    rangeArr = selRanges[i];
    start    = rangeArr[0];
    end      = rangeArr[1];

    var range = window.editorShell.editorDocument.createRange();

    node   = EditorGetNodeFromOffsets(start[0]);
    offset = start[1];

    range.setStart(node, offset);

    node   = EditorGetNodeFromOffsets(end[0]);
    offset = end[1];

    range.setEnd(node, offset);

    selection.addRange(range);
  }
}

function EditorTestSelection()
{
  if (window.editorShell)
  {
    dump("Testing selection\n");
    var selection = window.editorShell.editorSelection;
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
  if (window.editorShell) {
    dump("Running Unit Tests\n");
    window.editorShell.RunUnitTests();
  }
}

function EditorExit()
{
    if (window.editorShell) {
	    dump("Exiting\n");
      window.editorShell.Exit();
    }
}

function EditorTestDocument()
{
  if (window.editorShell)
  {
    dump("Getting document\n");
    var theDoc = window.editorShell.editorDocument;
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

