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

var gTagToFormat = {
    "P"          : "Normal",	// these should really be entities. Not sure how to do that from JS
    "H1"         : "Heading 1",
    "H2"         : "Heading 2",
    "H3"         : "Heading 3",
    "H4"         : "Heading 4",
    "H5"         : "Heading 5",
    "H6"         : "Heading 6",
    "BLOCKQUOTE" : "Blockquote",
    "ADDRESS"    : "Address",
    "PRE"        : "Preformatted",
    "LI"         : "List Item",
    "DT"         : "Definition Term",
    "DD"         : "Definition Description"
  };


var gStyleTags = {
    "bold"       : "b",
    "italic"     : "i",
    "underline"  : "u"
  };
  
  
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
  editorShell.SetDocumentCharacterSet(aCharset);
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
/* Testing returning out params
  var first = new Object();
  var all = new Object();
  var any = new Object();
  editorShell.GetTextProperty("tt", "", "", first, any, all);
  dump("GetTextProperty: first: "+first.value+", any: "+any.value+", all: "+all.value+"\n");
*/
  if( fontFace == "tt") {
    // The old "teletype" attribute
    editorShell.SetTextProperty("tt", "", "");  
    // Clear existing font face
    editorShell.RemoveTextProperty("font", "face");
  } else {
    // Remove any existing TT nodes
    editorShell.RemoveTextProperty("tt", "", "");  

    if( fontFace == "" || fontFace == "normal") {
      editorShell.RemoveTextProperty("font", "face");
    } else {
      editorShell.SetTextProperty("font", "face", fontFace);
    }
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

function EditorApplyStyle(tagName)
{
  dump("applying style\n");
  editorShell.SetTextProperty(tagName, "", "");
  contentWindow.focus();
}

function EditorRemoveStyle(tagName)
{
  editorShell.RemoveTextProperty(tagName, "");
  contentWindow.focus();
}

function EditorToggleStyle(styleName)
{
  // see if the style is already set by looking at the observer node,
  // which is the appropriate button
  var theButton = document.getElementById(styleName + "Button");
  
  if (theButton)
  {
    var isOn = theButton.getAttribute(styleName);
    if (isOn == "true")
      editorShell.RemoveTextProperty(gStyleTags[styleName], "", "");
    else
      editorShell.SetTextProperty(gStyleTags[styleName], "", "");

    contentWindow.focus();
  }
  else
  {
    dump("No button found for the " + styleName + " style");
  }
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
    var  outputHTML = editorShell.GetContentsAs("text/html", 0);
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
  window.openDialog("chrome://editor/content/EdLinkProps.xul","LinkDlg", "chrome,close,titlebar,modal");
  contentWindow.focus();
}

function EditorInsertImage()
{
  window.openDialog("chrome://editor/content/EdImageProps.xul","ImageDlg", "chrome,close,titlebar,modal");
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
    window.openDialog("chrome://editor/content/EdHLineProps.xul", "HLineDlg", "chrome,close,titlebar,modal");
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
  window.openDialog("chrome://editor/content/EdNamedAnchorProps.xul", "NamedAnchorDlg", "chrome,close,titlebar,modal", "");
  contentWindow.focus();
}

function EditorIndent(indent)
{
  dump("indenting\n");
  editorShell.Indent(indent);
  contentWindow.focus();
}

// Call this with insertAllowed = true to allow inserting if not in existing table,
//   else use false to do nothing if not in a table
function EditorInsertOrEditTable(insertAllowed)
{
  var selection = editorShell.editorSelection;
  dump("Selection: Anchor: "+selection.anchorNode+selection.anchorOffset+" Focus: "+selection.focusNode+selection.focusOffset+"\n");

  var table = editorShell.GetElementOrParentByTagName("table", null);
  if (table) {
    // Edit properties of existing table
    dump("Existing table found ... Editing its properties\n");

    window.openDialog("chrome://editor/content/EdTableProps.xul", "TableDlg", "chrome,close,titlebar,modal", "");
    contentWindow.focus();
  } else if(insertAllowed) {
    EditorInsertTable();
  }
}

function EditorInsertTable()
{
  // Insert a new table
  window.openDialog("chrome://editor/content/EdInsertTable.xul", "TableDlg", "chrome,close,titlebar,modal", "");
  contentWindow.focus();
}

function EditorInsertTableCell(after)
{
  editorShell.InsertTableCell(1,after);
  contentWindow.focus();
}

// Just insert before current row or column for now
function EditorInsertTableRow()
{
  editorShell.InsertTableRow(1,false);
  contentWindow.focus();
}

function EditorInsertTableColumn()
{
  editorShell.InsertTableColumn(1,false);
  contentWindow.focus();
}

function JoinTableCells()
{
  editorShell.JoinTableCells();
  contentWindow.focus();
}

function EditorDeleteTable()
{
  editorShell.DeleteTable();
  contentWindow.focus();
}

function EditorDeleteTableRow()
{
  editorShell.DeleteTableRow();
  contentWindow.focus();
}

function EditorDeleteTableColumn()
{
  editorShell.DeleteTableColumn();
  contentWindow.focus();
}


function EditorDeleteTableCell()
{
  editorShell.DeleteTableCell();
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
    buttonText = "Edit Mode";
  }
  else {
    EditorDisplayStyle = true;
    styleSheet = "chrome://editor/content/EditorContent.css"
    buttonText = "Preview";
  }
  //TODO: THIS IS NOT THE RIGHT THING TO DO!
  EditorApplyStyleSheet(styleSheet);
  
  button = document.getElementById("DisplayStyleButton");
  if (button)
    button.setAttribute("value",buttonText);
}

function EditorPrintPreview()
{
  window.openDialog("resource:/res/samples/printsetup.html", "PrintPreview", "chrome,close,titlebar", "");
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
      window.openDialog("chrome://editor/content/EdMessage.xul", "NoSpellError", "chrome,close,titlebar,modal", "", "No misspelled word was found.", "Check Spelling");
      spellChecker.CloseSpellChecking();
    } else {
      dump("We found a MISSPELLED WORD\n");
      // Set spellChecker variable on window
      window.spellChecker = spellChecker;
      window.openDialog("chrome://editor/content/EdSpellCheck.xul", "SpellDlg", "chrome,close,titlebar,modal", "", firstMisspelledWord);
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
  var lineNum     = 0;
  var ex;

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

    ++lineNum;

    try       { eval(buf.value); }
    catch(ex) { dump("Playback ERROR: Line " + lineNum + "  " + ex + "\n"); return; }
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
  contentWindow.focus();

  fs = null;
}

function EditorStopLog()
{
  editorShell.StopLogging();
  contentWindow.focus();
}

function EditorRunLog()
{
  var fs;
  fs = EditorGetScriptFileSpec();
  EditorExecuteScript(fs);
  contentWindow.focus();
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
  var saveButton = document.getElementById("SaveButton");
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

function EditorTestTableLayout()
{
  var table = editorShell.GetElementOrParentByTagName("table", null);
  if (!table) {
    dump("Enclosing Table not found: Place caret in a table cell to do this test\n\n");
    return;
  }
    
  var cell;
  var startRowIndexObj = new Object();
  var startColIndexObj = new Object();
  var rowSpanObj = new Object();
  var colSpanObj = new Object();
  var isSelectedObj = new Object();
  var startRowIndex = 0;
  var startColIndex = 0;
  var rowSpan;
  var colSpan;
  var isSelected;
  var col = 0;
  var row = 0;
  var rowCount = 0;
  var maxColCount = 0;
  var doneWithRow = false;
  var doneWithCol = false;

  dump("\n\n\n************ Starting Table Layout test ************\n");

  // Note: We could also get the number of rows, cols and use for loops,
  //   but this tests using out-of-bounds offsets to detect end of row or column

  while (!doneWithRow)  // Iterate through rows
  {  
    while(!doneWithCol)  // Iterate through cells in the row
    {
      try {
        cell = editorShell.GetCellDataAt(table, row, col, startRowIndexObj, startColIndexObj,
                                         rowSpanObj, colSpanObj, isSelectedObj);

        if (cell)
        {
          rowSpan = rowSpanObj.value;
          colSpan = colSpanObj.value;
          isSelected = isSelectedObj.value;
          
          dump("Row,Col: "+row+","+col+" StartRow,StartCol: "+startRowIndexObj.value+","+startColIndexObj.value+" RowSpan="+rowSpan+" ColSpan="+colSpan);
          if (isSelected)
            dump("  Cell is selected\n");
          else
            dump("  Cell is NOT selected\n");

          // Save the indexes of a cell that will span across the cellmap grid
          if (rowSpan > 1)
            startRowIndex = startRowIndexObj.value;
          if (colSpan > 1)
            startColIndex = startColIndexObj.value;

          // Initialize these for efficient spanned-cell search
          startRowIndexObj.value = startRowIndex;
          startColIndexObj.value = startColIndex;

          col++;
        } else {
          doneWithCol = true;
          // Get maximum number of cells in any row
          if (col > maxColCount)
            maxColCount = col;
          dump(" End of row found\n\n");
        }
      }
      catch (e) {
        dump("  *** GetCellDataAt barfed at Row,Col:"+row+","+col+" ***\n\n");
        col++;
      }
    }
    if (col == 0) {
      // Didn't find a cell in the first col of a row,
      // thus no more rows in table
      doneWithRow = true;
      rowCount = row;
      dump("No more rows in table\n\n");
    } else {
      // Setup for next row
      col = 0;
      row++;
      doneWithCol = false;
      dump("Setup for next row\n");
    }      
  }
  dump("Counted during scan: Number of rows="+rowCount+" Number of Columns="+maxColCount+"\n");
  rowCount = editorShell.GetTableRowCount(table);
  maxColCount = editorShell.GetTableColumnCount(table);
  dump("From nsITableLayout: Number of rows="+rowCount+" Number of Columns="+maxColCount+"\n****** End of Table Layout Test *****\n\n");
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
function onStyleChange(theStyle)
{
  var theButton = document.getElementById(theStyle + "Button");
  if (theButton)
  {
	var isOn = theButton.getAttribute(theStyle);
	if (isOn == "true") {
      theButton.setAttribute("toggled", 1);
    } else {
      theButton.setAttribute("toggled", 0);
    }
  }
}

function onDirtyChange()
{
  // this should happen through style, but that doesn't seem to work.
  var theButton = document.getElementById("SaveButton");
  if (theButton)
  {
	var isDirty = theButton.getAttribute("dirty");
    if (isDirty == "true") {
      theButton.setAttribute("src", "chrome://editor/skin/images/ED_SaveMod.gif");
    } else {
      theButton.setAttribute("src", "chrome://editor/skin/images/ED_SaveFile.gif");
    }
  }
}

function onParagraphFormatChange()
{
  var theButton = document.getElementById("ParagraphPopup");
  if (theButton)
  {
	var theFormat = theButton.getAttribute("format");
    theButton.setAttribute("value", gTagToFormat[theFormat]);
    dump("Setting value\n");
  }
}

function onListFormatChange(listType)
{
  var theButton = document.getElementById(listType + "Button");
  if (theButton)
  {
	var listFormat = theButton.getAttribute("format");
	if (listFormat == listType) {
      theButton.setAttribute("toggled", 1);
    } else {
      theButton.setAttribute("toggled", 0);
    }
  }
  
}
