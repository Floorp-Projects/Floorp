/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Main Composer window UI control */

var toolbar;
var documentModified;
var EditorDisplayStyle = true;

function EditorStartup(editorType)
{
  dump("Doing Startup...\n");
  contentWindow = window.content;

  // set up event listeners
  window.content.addEventListener("load", EditorDocumentLoaded, true, false);  
  
  dump("Trying to make an Editor Shell through the component manager...\n");
  editorShell = Components.classes["component://netscape/editor/editorshell"].createInstance();
  if (editorShell)
    editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
  
  if (!editorShell)
  {
    dump("Failed to create editor shell\n");
    // 7/12/99 THIS DOESN'T WORK YET!
    window.close();
    return;
  }
  
  // store the editor shell in the window, so that child windows can get to it.
  window.editorShell = editorShell;
  
  editorShell.Init();
  editorShell.SetWebShellWindow(window);
  editorShell.SetToolbarWindow(window)
  editorShell.SetEditorType(editorType);
  editorShell.SetContentWindow(contentWindow);

  // Get url for editor content and load it.
  // the editor gets instantiated by the editor shell when the URL has finished loading.
  var url = document.getElementById("args").getAttribute("value");
  editorShell.LoadUrl(url);
  
  dump("EditorAppCore windows have been set.\n");
  SetupToolbarElements();

  // Set focus to the edit window
  // This still doesn't work!
  // It works after using a toolbar button, however!
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
  editorShell.NewWindow();
}

function EditorOpen()
{
  dump("In EditorOpen..\n");
  editorShell.Open();
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
  dump("In EditorNewBrowser..\n");
 
  core = XPAppCoresManager.Find("toolkitCore");
  if ( !core ) {
    core = new ToolkitCore();
    if ( core ) {
      core.Init("toolkitCore");
    }
  }
  if ( core ) {
    core.ShowWindowWithArgs( "chrome://navigator/content/navigator.xul", window, "" );
  } else {
    dump("Error; can't create toolkitCore\n");
  }
}

function EditorSave()
{
  dump("In EditorSave...\n");
  editorShell.Save();
  contentWindow.focus();
}

function EditorSaveAs()
{
  dump("In EditorSave...\n");
  editorShell.SaveAs();
  contentWindow.focus();
}


function EditorPrint()
{
  dump("In EditorPrint..\n");
  editorShell.Print();
  contentWindow.focus();
}

function EditorClose()
{
  dump("In EditorClose...\n");
  editorShell.CloseWindow();
  // This doesn't work, but we can close
  //   the window in the EditorAppShell, so we don't need it
  //window.close();
}

// --------------------------- Edit menu ---------------------------

function EditorUndo()
{
  dump("Undoing\n");
  editorShell.Undo();
  contentWindow.focus();
}

function EditorRedo()
{
  dump("Redoing\n");
  editorShell.Redo();
  contentWindow.focus();
}

function EditorCut()
{
  editorShell.Cut();
  contentWindow.focus();
}

function EditorCopy()
{
  editorShell.Copy();
  contentWindow.focus();
}

function EditorPaste()
{
  editorShell.Paste();
  contentWindow.focus();
}

function EditorPasteAsQuotation()
{
  editorShell.PasteAsQuotation();
  contentWindow.focus();
}

function EditorPasteAsQuotationCited(citeString)
{
  editorShell.PasteAsCitedQuotation(CiteString);
  contentWindow.focus();
}

function EditorSelectAll()
{
  editorShell.SelectAll();
  contentWindow.focus();
}

function EditorFind()
{
  editorShell.Find();
  contentWindow.focus();
}

function EditorFindNext()
{
  editorShell.FindNext();
}

function EditorShowClipboard()
{
  dump("In EditorShowClipboard...\n");
}

// --------------------------- View menu ---------------------------

function EditorSetDocumentCharacterSet(aCharset)
{
  dump(aCharset);
  editorShell.editorDocument.SetDocumentCharacterSet(aCharset);
}


// --------------------------- Text style ---------------------------

function EditorSetTextProperty(property, attribute, value)
{
  editorShell.SetTextProperty(property, attribute, value);
  dump("Set text property -- calling focus()\n");
  contentWindow.focus();
}

function EditorSetParagraphFormat(paraFormat)
{
  editorShell.paragraphFormat = paraFormat;
  contentWindow.focus();
}

function EditorSetFontSize(size)
{
  if( size == "0" || size == "normal" || 
      size == "+0" )
  {
    editorShell.RemoveTextProperty("font", size);
    dump("Removing font size\n");
  } else {
    dump("Setting font size\n");
    editorShell.SetTextProperty("font", "size", size);
  }
  contentWindow.focus();
}

function EditorSetFontFace(fontFace)
{
  if( fontFace == "" || fontFace == "normal") {
    editorShell.RemoveTextProperty("font", "face");
  } else if( fontFace == "tt") {
    // The old "teletype" attribute
    editorShell.SetTextProperty("tt", "", "");  
    // Clear existing font face
    editorShell.RemoveTextProperty("font", "face");
  } else {
    editorShell.SetTextProperty("font", "face", fontFace);
  }        
  contentWindow.focus();
}

function EditorSetFontColor(color)
{
  editorShell.SetTextProperty("font", "color", color);
  contentWindow.focus();
}

function EditorSetBackgroundColor(color)
{
  editorShell.SetBackgroundColor(color);
  contentWindow.focus();
}

function EditorApplyStyle(styleName)
{
  dump("applying style\n");
  editorShell.SetTextProperty(styleName, "", "");
  contentWindow.focus();
}

function EditorRemoveStyle(styleName)
{
  editorShell.RemoveTextProperty(styleName, "");
  contentWindow.focus();
}

function EditorRemoveLinks()
{
  editorShell.RemoveTextProperty("a", "");
  contentWindow.focus();
}

function EditorApplyStyleSheet(styleSheetURL)
{
  editorShell.ApplyStyleSheet(styleSheetURL);
  contentWindow.focus();
}


// --------------------------- Output ---------------------------

function EditorGetText()
{
  if (editorShell) {
    dump("Getting text\n");
    var  outputText = editorShell.GetContentsAs("text/plain", 2);
    dump(outputText + "\n");
  }
}

function EditorGetHTML()
{
  if (editorShell) {
    dump("Getting HTML\n");
    var  outputHTML = editorShell.GetContentsAs("text/html", 2);
    dump(outputHTML + "\n");
  }
}

function EditorGetXIF()
{
  if (window.editorShell) {
    dump("Getting XIF\n");
    var  outputHTML = editorShell.GetContentsAs("text/xif", 2);
    dump(outputHTML + "\n");
  }
}

function EditorDumpContent()
{
  if (window.editorShell) {
    dump("==============  Content Tree: ================\n");
    window.editorShell.DumpContentTree();
  }
}

function EditorInsertText(textToInsert)
{
  editorShell.InsertText(textToInsert);
}

function EditorInsertHTML()
{
  window.openDialog("chrome://editor/content/EdInsSrc.xul","InsSrcDlg", "chrome", "");
}

function EditorInsertLink()
{
  window.openDialog("chrome://editor/content/EdLinkProps.xul","LinkDlg", "chrome", "");
  contentWindow.focus();
}

function EditorInsertImage()
{
  window.openDialog("chrome://editor/content/EdImageProps.xul","ImageDlg", "chrome", "");
  contentWindow.focus();
}

function EditorInsertTable()
{
  dump("Insert Table Dialog starting.\n");
  window.openDialog("chrome://editor/content/EdInsertTable.xul", "TableDlg", "chrome", "");
  contentWindow.focus();
}

function EditorInsertHLine()
{
  // Inserting an HLine is different in that we don't use properties dialog
  //  unless we are editing an existing line's attributes
  //  We get the last-used attributes from the prefs and insert immediately

  tagName = "hr";
  hLine = editorShell.GetSelectedElement(tagName);

  if (hLine) {
    // We only open the dialog for an existing HRule
    window.openDialog("chrome://editor/content/EdHLineProps.xul", "HLineDlg", "chrome", "");
  } else {
    hLine = editorShell.CreateElementWithDefaults(tagName);
    if (hLine) {
      editorShell.InsertElement(hLine, false);
    }
  }
  contentWindow.focus();
}

function EditorInsertNamedAnchor()
{
  window.openDialog("chrome://editor/content/EdNamedAnchorProps.xul", "NamedAnchorDlg", "chrome", "");
  contentWindow.focus();
}

function EditorIndent(indent)
{
  dump("indenting\n");
  editorShell.Indent(indent);
  contentWindow.focus();
}

function EditorInsertList(listType)
{
  dump("Inserting list\n");
  editorShell.InsertList(listType);
  contentWindow.focus();
}

function EditorAlign(align)
{
  dump("aligning\n");
  editorShell.Align(align);
  contentWindow.focus();
}

function EditorToggleDisplayStyle()
{
  if (EditorDisplayStyle) {
    EditorDisplayStyle = false;
    styleSheet = "resource:/res/ua.css";
    //TODO: Where do we store localizable JS strings?
    buttonText = "Preview";
  }
  else {
    EditorDisplayStyle = true;
    styleSheet = "chrome://editor/content/EditorContent.css"
    buttonText = "Edit Mode";
  }
  EditorApplyStyleSheet(styleSheet);
  
  button = document.getElementById("DisplayStyleButton");
  if (button)
    button.setAttribute("value",buttonText);
}

function EditorPrintPreview()
{
  window.openDialog("resource:/res/samples/printsetup.html", "PrintPreview", "chrome", "");
  contentWindow.focus();
}

function CheckSpelling()
{
  var spellChecker = editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
  if (spellChecker)
  {
    dump("Check Spelling starting...\n");
    // Start the spell checker module. Return is first misspelled word
    firstMisspelledWord = spellChecker.StartSpellChecking();
    dump(firstMisspelledWord+"\n");
    if( firstMisspelledWord == "")
    {
      // No misspelled word - tell user
      window.openDialog("chrome://editor/content/EdMessage.xul", "NoSpellError", "chrome", "", "No misspelled word was found.", "Check Spelling");
      spellChecker.CloseSpellChecking();
    } else {
      dump("We found a MISSPELLED WORD\n");
      // Set spellChecker variable on window
      window.spellChecker = spellChecker;
      window.openDialog("chrome://editor/content/EdSpellCheck.xul", "SpellDlg", "chrome", "", firstMisspelledWord);
    }
  }
  contentWindow.focus();
}

function OnCreateAlignmentPopup()
{
  dump("Creating Alignment popup window\n");
}
  
// --------------------------- Debug stuff ---------------------------

function EditorApplyStyleSheet(url)
{
  if (editorShell)
  {
    editorShell.ApplyStyleSheet(url);
  }
}

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

  fs = EditorGetScriptFileSpec();
  editorShell.StartLogging(fs);

  fs = null;
}

function EditorStopLog()
{
  editorShell.StopLogging();
}

function EditorRunLog()
{
  var fs;
  fs = EditorGetScriptFileSpec();
  EditorExecuteScript(fs);
}

function EditorDocumentLoaded()
{
  dump("The document was loaded in the content area\n");

  //window.content.addEventListener("keyup", EditorReflectDocState, true, false);	// post process, no capture
  //window.content.addEventListener("dblclick", EditorDoDoubleClick, true, false);

  documentModified = (window.editorShell.documentStatus != 0);
  return true;
}

function UpdateSaveButton(modified)
{
  var saveButton = document.getElementById("saveButton");
   if (saveButton)
  {
    if (modified) {
      saveButton.setAttribute("src", "chrome://editor/skin/images/ED_SaveMod.gif");
    } else {
      saveButton.setAttribute("src", "chrome://editor/skin/images/ED_SaveFile.gif");
    }
    dump("updating button\n");
  }
  else
  {
    dump("could not find button\n");
  }
}

function EditorDoDoubleClick()
{
  dump("doing double click\n");
}

function EditorReflectDocState()
{
  var docState = window.editorShell.documentStatus;
  var stateString;
  
  if (docState == 0) {
    stateString = "unmodified";
  } else {
    stateString = "modified";
  }
  
  var oldModified = documentModified;
  documentModified = (window.editorShell.documentStatus != 0);
  
  if (oldModified != documentModified)
    UpdateSaveButton(documentModified);
  
  dump("Updating save state " + stateString + "\n");
  
  return true;
}

function EditorDocStateChanged()
{
}

function EditorGetNodeFromOffsets(offsets)
{
  var node = null;
  var i;

  node = editorShell.editorDocument;

  for (i = 0; i < offsets.length; i++)
  {
    node = node.childNodes[offsets[i]];
  }

  return node;
}

function EditorSetSelectionFromOffsets(selRanges)
{
  var rangeArr, start, end, i, node, offset;
  var selection = editorShell.editorSelection;

  selection.clearSelection();

  for (i = 0; i < selRanges.length; i++)
  {
    rangeArr = selRanges[i];
    start    = rangeArr[0];
    end      = rangeArr[1];

    var range = editorShell.editorDocument.createRange();

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
  dump("Testing selection\n");
  var selection = editorShell.editorSelection;
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

  var output;

  dump("\n====== Selection as XIF =======================\n");
  output = editorShell.GetContentsAs("text/xif", 1);
  dump(output + "\n\n");

  dump("====== Selection as unformatted text ==========\n");
  output = editorShell.GetContentsAs("text/plain", 1);
  dump(output + "\n\n");

  dump("====== Selection as formatted text ============\n");
  output = editorShell.GetContentsAs("text/plain", 3);
  dump(output + "\n\n");

  dump("====== Selection as HTML ======================\n");
  output = editorShell.GetContentsAs("text/html", 1);
  dump(output + "\n\n");
}

function EditorShowEmbeddedObjects()
{
  dump("\nEmbedded Objects:\n");
  var objectArray = editorShell.GetEmbeddedObjects();
  dump(objectArray.Count() + " embedded objects\n");
  for (var i=0; i < objectArray.Count(); ++i)
    dump(objectArray.GetElementAt(i) + "\n");
}

function EditorUnitTests()
{
  dump("Running Unit Tests\n");
  editorShell.RunUnitTests();
}

function EditorExit()
{
	dump("Exiting\n");
  editorShell.Exit();
}

function EditorTestDocument()
{
  dump("Getting document\n");
  var theDoc = editorShell.editorDocument;
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
  var boldButton = document.getElementByID("BoldButton");
  if (boldButton)
  {
	bold = boldButton.getAttribute("bold");
    if ( bold == "true" ) {
      boldButton.setAttribute( "disabled", false );
	} else {
	  boldButton.setAttribute( "disabled", true );
	}
  }
	dump("  Bold state changed\n");
}

