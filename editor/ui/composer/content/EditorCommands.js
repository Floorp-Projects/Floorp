/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *    Sammy Ford
 *    Dan Haddix (dan6992@hotmail.com)
 */

/* Main Composer window UI control */

var editorShell;
var toolbar;
var documentModified;
var EditorDisplayMode = 0;  // Normal Editor mode

// These must be kept in synch with the XUL <options> lists
var gParagraphTagNames = new Array("P","H1","H2","H3","H4","H5","H6","BLOCKQUOTE","ADDRESS","PRE","DT","DD");
var gFontFaceNames = new Array("","tt","Arial, Helvetica","Times","Courier");
var gFontSizeNames = new Array("-2","-1","0","+1","+2","+3","+4");

var gStyleTags = {
    "bold"       : "b",
    "italic"     : "i",
    "underline"  : "u"
  };
  
function EditorOnLoad() {
    // See if argument was passed.
    if ( window.arguments && window.arguments[0] ) {
        // Opened via window.openDialog with URL as argument.    
        // Put argument where EditorStartup expects it.
        document.getElementById( "args" ).setAttribute( "value", window.arguments[0] );
    }
    // Continue with normal startup.
    EditorStartup( 'html', document.getElementById("content-frame"));
    window.tryToClose = EditorClose;
    return;
}
  
function TextEditorOnLoad() {
    // See if argument was passed.
    if ( window.arguments && window.arguments[0] ) {
        // Opened via window.openDialog with URL as argument.    
        // Put argument where EditorStartup expects it.
        document.getElementById( "args" ).setAttribute( "value", window.arguments[0] );
    }
    // Continue with normal startup.
    EditorStartup( 'text', document.getElementById("content-frame"));
    return;
}
  
function EditorStartup(editorType, editorElement)
{
  dump("Doing Editor Startup...\n");
  contentWindow = window.content;

  // set up event listeners
  window.addEventListener("load", EditorDocumentLoaded, true, false);  
  
  dump("Trying to make an Editor Shell through the component manager...\n");

  // store the editor shell in the window, so that child windows can get to it.
  var editorShell = window.editorShell = editorElement.editorShell;
  
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

function TestMenuCreation()
{
  var menubar = document.getElementById("MainMenuBar");
  var fileMenu = menubar.firstChild;
  var filePopup = fileMenu.firstChild;
  
  dump("File menu "+ fileMenu + "\n");
  
  var newChild = document.createElement("menuitem");
  newChild.setAttribute("value", "testing");
  newChild.setAttribute("id", "testItem");
  dump(newChild);
  filePopup.appendChild(newChild);
  
  // now try and find it
  var someItem = document.getElementById("testItem");
  if (!someItem)
    dump("Failed to find new menu item\n");
}

// NOT USED?
function GenerateFormatToolbar()
{
  var toolbarButtons = [
  
    // bold button
    {
      "id"          : "boldButton",
      "value"       : "B",
      "onclick"     : "EditorToggleStyle('bold')"
    },
    
    // italics button
    {
      "id"          : "italicButton",
      "value"       : "I"
    },
  
    // underline button
    {
      "id"          : "underlineButton",
      "value"       : "U"
    },

    // ul button
    {
      "id"          : "ulButton",
      "src"         : "chrome://editor/skin/images/ED_Bullets.gif",
      "align"       : "bottom"
    },

    // ol button
    {
      "id"          : "olButton",
      "src"         : "chrome://editor/skin/images/ED_Numbers.gif",
      "align"       : "bottom"
    },
  
  ];
  
  
  var toolbar = document.getElementById("FormatToolbar");

  for (i = 0; i < toolbarButtons.length; i ++)
  {
    var newChild = document.createElement("titledbutton");
    var thisButton = toolbarButtons[i];
    
    for (prop in thisButton)
    {
      var theValue = thisButton[prop];
      newChild.setAttribute(prop, theValue);
    }
    
    toolbar.appendChild(newChild);
  }
  
  // append a spring
  
  var theSpring = document.createElement("spring");
  theSpring.setAttribute("flex", "100%");
  toolbar.appendChild(theSpring);

  // testing
  var boldButton = document.getElementById("boldButton");
  if (!boldButton)
    dump("Failed to find button\n");
  
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

function _EditorNotImplemented()
{
  dump("Function not implemented\n");
}

function EditorShutdown()
{
  dump("In EditorShutdown..\n");
  //editorShell = XPAppCoresManager.Remove(editorShell);
}


// --------------------------- File menu ---------------------------

function EditorOpen()
{
  dump("In EditorOpen..\n");
  editorShell.Open();
}

function EditorNewPlaintext()
{
  window.openDialog( "chrome://editor/content/TextEditorAppShell.xul",
                     "_blank",
                     "chrome,dialog=no,all",
                     "chrome://editor/content/EditorInitPagePlain.html");
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
  return editorShell.CloseWindow();
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

function EditorViewSource()
{
  window.openDialog( "chrome://editor/content/viewsource.xul", "_blank", "all,dialog=no", window.content.location );
}


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

function EditorSelectParagraphFormat()
{
  var select = document.getElementById("ParagraphSelect");
  if (select)
  { 
    if (select.selectedIndex == -1)
      return;
    editorShell.SetParagraphFormat(gParagraphTagNames[select.selectedIndex]);
  }
}

function onParagraphFormatChange()
{
  var select = document.getElementById("ParagraphSelect");
  if (select)
  { 
    // If we don't match anything, set to "normal"
    var newIndex = 0;
  	var format = select.getAttribute("format");
    if ( format == "mixed")
    {
      // No single type selected
      newIndex = -1;
    }
    else
    {
      for( var i = 0; i < gParagraphTagNames.length; i++)
      {
        if( gParagraphTagNames[i] == format )
        {
          newIndex = i;
          break;
        }
      }
    }
    if (select.selectedIndex != newIndex)
      select.selectedIndex = newIndex;
  }
}

function EditorSetParagraphFormat(paraFormat)
{
  editorShell.SetParagraphFormat(paraFormat);
  contentWindow.focus();
}

function EditorSelectFontFace() 
{
  var select = document.getElementById("FontFaceSelect");
//dump("EditorSelectFontFace: "+gFontFaceNames[select.selectedIndex]+"\n");
  if (select)
  { 
    if (select.selectedIndex == -1)
      return;

    EditorSetFontFace(gFontFaceNames[select.selectedIndex]);
  }
}

function onFontFaceChange()
{
  var select = document.getElementById("FontFaceSelect");
  if (select)
  { 
    // Default selects "Variable Width"
    var newIndex = 0;
  	var face = select.getAttribute("face");
//dump("onFontFaceChange: face="+face+"\n");
    if ( face == "mixed")
    {
      // No single type selected
      newIndex = -1;
    }
    else 
    {
      for( var i = 0; i < gFontFaceNames.length; i++)
      {
        if( gFontFaceNames[i] == face )
        {
          newIndex = i;
          break;
        }
      }
    }
    if (select.selectedIndex != newIndex)
      select.selectedIndex = newIndex;
  }
}

function EditorSetFontFace(fontFace)
{
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

function EditorSelectFontSize()
{
  var select = document.getElementById("FontSizeSelect");
dump("EditorSelectFontSize: "+gFontSizeNames[select.selectedIndex]+"\n");
  if (select)
  { 
    if (select.selectedIndex == -1)
      return;

    EditorSetFontSize(gFontSizeNames[select.selectedIndex]);
  }
}

function onFontSizeChange()
{
  var select = document.getElementById("FontFaceSelect");
  if (select)
  { 
    // If we don't match anything, set to "0 (normal)"
    var newIndex = 2;
  	var size = select.getAttribute("size");
    if ( size == "mixed")
    {
      // No single type selected
      newIndex = -1;
    }
    else 
    {
      for( var i = 0; i < gFontSizeNames.length; i++)
      {
        if( gFontSizeNames[i] == size )
        {
          newIndex = i;
          break;
        }
      }
    }
    if (select.selectedIndex != newIndex)
      select.selectedIndex = newIndex;
  }
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

function EditorIncreaseFontSize()
{
  editorShell.IncreaseFontSize();
}

function EditorDecreaseFontSize()
{
  editorShell.DecreaseFontSize();
}

function EditorSelectTextColor()
{
  dump("EditorSelectTextColor\n");
}

function EditorSelectBackColor()
{
  dump("EditorSelectBackColor\n");
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
  // cmanske: I don't think we should depend on button state!
  //  (this won't work for other list styles, anyway)
  //  We need to get list type from document
  var theButton = document.getElementById(styleName + "Button");
  dump("Toggling style " + styleName + "\n");
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
    dump("No button found for the " + styleName + " style\n");
  }
}

function EditorRemoveLinks()
{
  editorShell.RemoveTextProperty("a", "");
  contentWindow.focus();
}

function EditorListProperties()
{
  window.openDialog("chrome://editor/content/EdListProps.xul","_blank", "chrome,close,titlebar,modal");
  contentWindow.focus();
}

function EditorApplyStyleSheet(styleSheetURL)
{
  // Second param is true for "override" type of sheet
  // We don't let users use that option
  editorShell.ApplyStyleSheet(styleSheetURL);
  contentWindow.focus();
}

function EditorImageMap()
{
  if (editorShell){
    var tagName = "img";
    image = editorShell.GetSelectedElement(tagName);

    //Test selected element to see if it's an image
    if (image){
      //If it is, launch image map dialog
      window.openDialog("chrome://editor/content/EdImageMap.xul", "_blank", "chrome,close", "");
    }
  }
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
  window.openDialog("chrome://editor/content/EdInsSrc.xul","_blank", "chrome,close,titlebar,modal", "");
  contentWindow.focus();
}

function EditorInsertLink()
{
  window.openDialog("chrome://editor/content/EdLinkProps.xul","_blank", "chrome,close,titlebar,modal");
  contentWindow.focus();
}

function EditorInsertImage()
{
  window.openDialog("chrome://editor/content/EdImageProps.xul","_blank", "chrome,close,titlebar,modal");
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
    dump("HLine was found -- opening dialog...!\n");

    // We only open the dialog for an existing HRule
    window.openDialog("chrome://editor/content/EdHLineProps.xul", "_blank", "chrome,close,titlebar,modal");
  } else {
    hLine = editorShell.CreateElementWithDefaults(tagName);

    if (hLine) {
      // We change the default attributes to those saved in the user prefs
      var prefs = Components.classes['component://netscape/preferences'];
      if (prefs) {
        prefs = prefs.getService();
      }
      if (prefs) {
        prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
      }
      if (prefs) {
        dump(" We found the Prefs Service\n");
        var percent;
        var height;
        var shading;
        var ud = "undefined";

        try {
          var align = prefs.GetIntPref("editor.hrule.align");
          dump("Align pref: "+align+"\n");
          if (align == 0 ) {
            hLine.setAttribute("align", "left");
          } else if (align == 2) {
            hLine.setAttribute("align", "right");
          } else  {
            // Default is center
            hLine.setAttribute("align", "center");
          }
	  
          var width = prefs.GetIntPref("editor.hrule.width");
          var percent = prefs.GetBoolPref("editor.hrule.width_percent");
          dump("Width pref: "+width+", percent:"+percent+"\n");
          if (percent)
            width = width +"%";

          hLine.setAttribute("width", width);

          var height = prefs.GetIntPref("editor.hrule.height");
          dump("Size pref: "+height+"\n");
          hLine.setAttribute("size", String(height));

          var shading = prefs.GetBoolPref("editor.hrule.shading");
          dump("Shading pref:"+shading+"\n");
          if (shading) {
            hLine.removeAttribute("noshade");
          } else {
            hLine.setAttribute("noshade", "");
          }
        }
        catch (ex) {
          dump("failed to get HLine prefs\n");
        }
      }
      try {
        editorShell.InsertElementAtSelection(hLine, true);
      } catch (e) {
        dump("Exception occured in InsertElementAtSelection\n");
      }
    }
  }
  contentWindow.focus();
}

function EditorInsertNamedAnchor()
{
  window.openDialog("chrome://editor/content/EdNamedAnchorProps.xul", "_blank", "chrome,close,titlebar,modal", "");
  contentWindow.focus();
}

function EditorIndent(indent)
{
  dump(indent+"\n");
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

    window.openDialog("chrome://editor/content/EdTableProps.xul", "_blank", "chrome,close,titlebar,modal", "");
    contentWindow.focus();
  } else if(insertAllowed) {
    EditorInsertTable();
  }
}

function EditorInsertTable()
{
  // Insert a new table
  window.openDialog("chrome://editor/content/EdInsertTable.xul", "_blank", "chrome,close,titlebar,modal", "");
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

function EditorMakeOrChangeList(listType)
{
  // check the observer node,
  // which is the appropriate button
  var theButton = document.getElementById(listType + "Button");
  dump("Toggling list " + listType + "\n");
  if (theButton)
  {
    var isOn = theButton.getAttribute("toggled");
    if (isOn == 1)
    {
      dump("Removing list \n");
      editorShell.RemoveList(listType);
    }
    else
      editorShell.MakeOrChangeList(listType);

    contentWindow.focus();
  }
  else
  {
    dump("No button found for the " + listType + " style\n");
  }
}

function EditorAlign(align)
{
  dump("aligning\n");
  editorShell.Align(align);
  contentWindow.focus();
}

function EditorSetDisplayStyle(mode)
{
  EditorDisplayMode = mode;
  editorShell.SetDisplayMode(mode);
  editButton = document.getElementById("EditModeButton");
  browserButton = document.getElementById("BrowserModeButton");
  var editSelected = 0;
  var browserSelected = 0;
  if (mode == 0) editSelected = 1;
  if (mode == 1) browserSelected = 1;
  dump(editButton+browserButton+" Display mode: EditSelected="+editSelected+" BrowserSelected="+browserSelected+"\n");
  editButton.setAttribute("selected",Number(editSelected));
  browserButton.setAttribute("selected",Number(browserSelected));
}

function EditorPrintPreview()
{
  window.openDialog("resource:/res/samples/printsetup.html", "_blank", "chrome,close,titlebar", "");
  contentWindow.focus();
}

function CheckSpelling()
{
  var spellChecker = editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
  if (spellChecker)
  {
    dump("Check Spelling starting...\n");
    // Start the spell checker module. Return is first misspelled word
    try {
      firstMisspelledWord = spellChecker.StartSpellChecking();
      dump(firstMisspelledWord+"\n");
      if( firstMisspelledWord == "")
      {
        // No misspelled word - tell user
        window.openDialog("chrome://editor/content/EdMessage.xul", "_blank", 
                          "chrome,close,titlebar,modal", "",
                          editorShell.GetString("NoMisspelledWord"), 
                          editorShell.GetString("CheckSpelling"));
      
        spellChecker.CloseSpellChecking();
      } else {
        dump("We found a MISSPELLED WORD\n");
        // Set spellChecker variable on window
        window.spellChecker = spellChecker;
        window.openDialog("chrome://editor/content/EdSpellCheck.xul", "_blank", "chrome,close,titlebar,modal", "", firstMisspelledWord);
      }
    }
    catch(ex) { 
      dump("*** Exception error from Spell Checker\n");
    }
  }
  contentWindow.focus();
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
  fs.unixStyleFilePath = "journal.js";
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

  //window.addEventListener("keyup", EditorReflectDocState, true, false);	// post process, no capture
  //window.addEventListener("dblclick", EditorDoDoubleClick, true, false);

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
  if (!selection)
  {
    dump("No selection!\n");
    return;
  }

  dump("Selection contains:\n");
  dump(selection.toString() + "\n");

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

  dump("====== Length and status =====================\n");
  output = "Document is ";
  if (editorShell.documentIsEmpty)
    output += "empty\n";
  else
    output += "not empty\n";
  output += "Document length is " + editorShell.documentLength + " characters";
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
  // This is in globalOverlay.js
  goQuitApplication();
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
    alert( "EditorCommands.js OpenFile should not be called!" );
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
  var theButton = document.getElementById("saveButton");
  if (theButton)
  {
    var isDirty = theButton.getAttribute("dirty");
    if (isDirty == "true") {
      theButton.setAttribute("src", "chrome://editor/skin/images/savemod.gif");
    } else {
      theButton.setAttribute("src", "chrome://editor/skin/images/savefile.gif");
    }
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
