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
 *    John Ratke (jratke@owc.net)
 */

/* Main Composer window UI control */

var editorShell;
var documentModified;
var prefAuthorString = "";
var EditorDisplayMode = 1;  // Normal Editor mode
var WebCompose = false;     // Set true for Web Composer, leave false for Messenger Composer

// These must be kept in synch with the XUL <options> lists
var gParagraphTagNames = new Array("","P","H1","H2","H3","H4","H5","H6","BLOCKQUOTE","ADDRESS","PRE","DT","DD");
var gFontFaceNames = new Array("","tt","Arial, Helvetica","Times","Courier");
var gFontSizeNames = new Array("xx-small","x-small","small","medium","large","x-large","xx-large");

var gStyleTags = {
    "bold"       : "b",
    "italic"     : "i",
    "underline"  : "u"
  };

const nsIFilePicker = Components.interfaces.nsIFilePicker;
  
function EditorOnLoad()
{
    // See if argument was passed.
    if ( window.arguments && window.arguments[0] ) {
        // Opened via window.openDialog with URL as argument.    
        // Put argument where EditorStartup expects it.
        document.getElementById( "args" ).setAttribute( "value", window.arguments[0] );
    }
    
    // Continue with normal startup.
    EditorStartup('html', document.getElementById("content-frame"));

    // Active menu items that are initially hidden in XUL
    //  because they are not needed by Messenger Composer
    var styleMenu = document.getElementById("stylesheetMenu")
    if (styleMenu)
      styleMenu.removeAttribute("hidden");

    WebCompose = true;
    window.tryToClose = EditorClose;
}
  
function TextEditorOnLoad()
{
    // See if argument was passed.
    if ( window.arguments && window.arguments[0] ) {
        // Opened via window.openDialog with URL as argument.    
        // Put argument where EditorStartup expects it.
        document.getElementById( "args" ).setAttribute( "value", window.arguments[0] );
    }
    // Continue with normal startup.
    EditorStartup('text', document.getElementById("content-frame"));
}

// This is called when the real editor document is created,
// before it's loaded.
var DocumentStateListener =
{
  NotifyDocumentCreated: function() 
  {
    EditorSetDefaultPrefs();
    // Call EditorSetDefaultPrefs first
    //  so it gets the default author before initing toolbars
    EditorInitToolbars();
  },
  NotifyDocumentWillBeDestroyed: function() {},
  NotifyDocumentStateChanged:function( isNowDirty ) {}
};

function EditorStartup(editorType, editorElement)
{
  contentWindow = window.content;
  
  // store the editor shell in the window, so that child windows can get to it.
  editorShell = editorElement.editorShell;
  
  editorShell.Init();
  editorShell.SetWebShellWindow(window);
  editorShell.SetToolbarWindow(window)
  editorShell.SetEditorType(editorType);
  editorShell.SetContentWindow(contentWindow);

  // add a listener to be called when document is really done loading
  editorShell.RegisterDocumentStateListener( DocumentStateListener );
 
  // Get url for editor content and load it.
  // the editor gets instantiated by the editor shell when the URL has finished loading.
  var url = document.getElementById("args").getAttribute("value");
  editorShell.LoadUrl(url);
  
  // Set focus to the edit window
  // This still doesn't work!
  // It works after using a toolbar button, however!
  contentWindow.focus();
}


function _EditorNotImplemented()
{
  dump("Function not implemented\n");
}

function EditorShutdown()
{
  dump("In EditorShutdown..\n");
}


// -------------------------- Key Bindings -------------------------

// ------------------------- Move Selection ------------------------
function SelectionBeginningOfLine()
{
  var selCont = editorShell.selectionController;
  selCont.intraLineMove(false, false);
}

function SelectionEndOfLine()
{
  var selCont = editorShell.selectionController;
  selCont.intraLineMove(true, false);
}

function SelectionBackwardChar()
{
  var selCont = editorShell.selectionController;
  selCont.characterMove(false, false);
}

function SelectionForwardChar()
{
  var selCont = editorShell.selectionController;
  selCont.characterMove(true, false);
}

function SelectionBackwardWord()
{
  var selCont = editorShell.selectionController;
  selCont.wordMove(false, false);
}

function SelectionForwardWord()
{
  var selCont = editorShell.selectionController;
  selCont.wordMove(true, false);
}

function SelectionPreviousLine()
{
  var selCont = editorShell.selectionController;
  selCont.lineMove(false, false);
}

function SelectionNextLine()
{
  var selCont = editorShell.selectionController;
  selCont.lineMove(true, false);
}

function SelectionPageUp()
{
  var selCont = editorShell.selectionController;
  selCont.pageMove(false, false);
}

function SelectionPageDown()
{
  var selCont = editorShell.selectionController;
  selCont.pageMove(true, false);
}

// --------------------------- Deletion --------------------------

function EditorDeleteCharForward()
{
  editorShell.DeleteCharForward();
}

function EditorDeleteCharBackward()
{
  editorShell.DeleteCharBackward();
}

function EditorDeleteWordForward()
{
  editorShell.DeleteWordForward();
}

function EditorDeleteWordBackward()
{
  editorShell.DeleteWordBackward();
}

function EditorDeleteToEndOfLine()
{
  editorShell.DeleteToEndOfLine();
}

function FindAndSelectEditorWindowWithURL(urlToMatch)
{
  if (!urlToMatch || urlToMatch.length == 0)
    return false;

  // returns true if found; false if an error or not found
  
  var windowManager = Components.classes["component://netscape/rdf/datasource?name=window-mediator"].getService();
  if ( !windowManager )
    return false;
  
  var windowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator);
  if ( !windowManagerInterface )
    return false;

  var enumerator = windowManagerInterface.getEnumerator( "composer:html" );
  if ( !enumerator )
    return false;

  while ( enumerator.HasMoreElements() )
  {
    var window = windowManagerInterface.convertISupportsToDOMWindow( enumerator.GetNext() );
    if ( window )
    {
      var didFindWindow = editorShell.checkOpenWindowForURLMatch(urlToMatch, window)
	    if (didFindWindow)
	    {
	      window.focus();
	      return true;
	    }
	  }
  }

  // not found
  return false;
}

// --------------------------- File menu ---------------------------


function EditorOpen()
{
  dump("In EditorOpen..\n");

  var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
  fp.init(window, editorShell.GetString("OpenHTMLFile"), nsIFilePicker.modeOpen);

  // While we include "All", include filters that prefer HTML and Text files
  fp.setFilters(nsIFilePicker.filterText | nsIFilePicker.filterHTML | nsIFilePicker.filterAll);

  /* doesn't handle *.shtml files */
  try {
    fp.show();
    /* need to handle cancel (uncaught exception at present) */
  }
  catch (ex) {
    dump("filePicker.chooseInputFile threw an exception\n");
  }

  /* check for already open window and activate it... 
   * note that we have to test the native path length
   *  since fileURL.spec will be "file:///" if no filename picked (Cancel button used)
  */
  if (fp.file && fp.file.path.length > 0) {

    var found = FindAndSelectEditorWindowWithURL(fp.fileURL.spec);
    if (!found) {
      /* open new window */
      window.openDialog("chrome://editor/content",
                        "_blank",
                        "chrome,dialog=no,all",
                        fp.fileURL.spec);
    }
  }
}

function EditorOpenRemote()
{
  /* The last parameter is the current browser window.
     Use 0 and the default checkbox will be to load into an editor
     and loading into existing browser option is removed
   */
  window.openDialog( "chrome://navigator/content/openLocation.xul", "_blank", "chrome,modal", 0);
}

// used by openLocation. see navigator.js for additional notes.
function delayedOpenWindow(chrome,flags,url) {
  setTimeout("window.openDialog('"+chrome+"','_blank','"+flags+"','"+url+"')", 10);
}

function EditorNewPlaintext()
{
  window.openDialog( "chrome://editor/content/TextEditorAppShell.xul",
                     "_blank",
                     "chrome,dialog=no,all",
                     "about:blank");
}

// returns wasSavedSuccessfully
function EditorSaveDocument(doSaveAs, doSaveCopy)
{
  //TODO: Replace this with nsIFilePicker code.
  // but should we try to do it all here or just use nsIFilePicker in editorShell code?
  editorShell.saveDocument(doSaveAs, doSaveCopy);
}

function EditorSave()
{
  dump("In EditorSave...\n");
  EditorSaveDocument(false, false)
  contentWindow.focus();
}

function EditorSaveAs()
{
  EditorSaveDocument(true, false);
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
}

// Check for changes to document and allow saving before closing
// This is hooked up to the OS's window close widget (e.g., "X" for Windows)
function EditorCanClose()
{
  // Returns FALSE only if user cancels save action
  return editorShell.CheckAndSaveDocument(editorShell.GetString("BeforeClosing"));
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
  dump("EditorShowClipboard not implemented\n");
}

// --------------------------- View menu ---------------------------

function EditorViewSource()
{
  // Temporary hack: save to a file and call up the source view window
  // using the local file url.
  if (!editorShell.CheckAndSaveDocument(editorShell.GetString("BeforeViewSource")))
    return;

  fileurl = "";
  try {
    fileurl = window.content.location;
  } catch (e) {
    return;
  }

  // CheckAndSave doesn't tell us if the user said "Don't Save",
  // so make sure we have a url:
  if (fileurl != "" && fileurl != "about:blank")
  {
    // Use a browser window to view source
    window.openDialog( "chrome://navigator/content/viewSource.xul",
                       "_blank",
                       "chrome,menubar,status,dialog=no,resizable",
                       fileurl,
                       "view-source" );
  }
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
  contentWindow.focus();
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
// dump("Mixed paragraph format *******\n");
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
  contentWindow.focus();
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
dump("Setting focus to content window...\n");
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
      size == "medium" )
  {
    editorShell.RemoveTextProperty("font", size);
    dump("Removing font size\n");
  } else {
    dump("Setting font size\n");
    // Temp: convert from new CSS size strings to old HTML size strings
    switch (size)
    {
      case "xx-small":
      case "x-small":
        size = "-2";
        break;
      case "small":
        size = "-1";
        break;
      case "large":
        size = "+1";
        break;
      case "x-large":
        size = "+2";
        break;
      case "xx-large":
        size = "+3";
        break;
    }
    editorShell.SetTextProperty("font", "size", size);
  }
/*
  BIG BUG! Setting <span> tag is totally horked -- stick with <font size> for beta1
  {
    // XXX-THIS IS DEFINITELY WRONG! 
    // We need to parse the style tag to set/remove only the "font-size"
    // TODO: We need a general SetInlineStyle(), RemoveInlineStyle() interface
    editorShell.RemoveTextProperty("span", "style");
    dump("Removing font size\n");
  } else {
    dump("Setting font size to: "+size+"\n");
    editorShell.SetTextProperty("span", "style", "font-size:"+size);
  }
*/
  contentWindow.focus();
}

function EditorIncreaseFontSize()
{
  editorShell.IncreaseFontSize();
  contentWindow.focus();
}

function EditorDecreaseFontSize()
{
  editorShell.DecreaseFontSize();
  contentWindow.focus();
}

function EditorSelectTextColor(ColorPickerID, ColorWellID)
{
  var color = getColorAndSetColorWell(ColorPickerID, ColorWellID);
  dump("EditorSelectTextColor: "+color+"\n");

  // Close appropriate menupopup  
  var menupopup;
  if (ColorPickerID == "menuTextCP")
    menupopup = document.getElementById("formatMenuPopup");
  else if (ColorPickerID == "TextColorPicker")
    menupopup = document.getElementById("TextColorPopup");

  if (menupopup) menupopup.closePopup();

  EditorSetFontColor(color);
  contentWindow.focus();
}

function EditorRemoveTextColor(ColorWellID)
{
  if (ColorWellID)
  {
    var menupopup = document.getElementById("TextColorPopup");
    if (menupopup) menupopup.closePopup();
  }

  //TODO: Set colorwell to browser's default color
  editorShell.SetTextProperty("font", "color", "");
  contentWindow.focus();
}

function EditorSelectBackColor(ColorPickerID, ColorWellID)
{
  var color = getColorAndSetColorWell(ColorPickerID, ColorWellID);
dump("EditorSelectBackColor: "+color+"\n");

  // Close appropriate menupopup  
  var menupopup;
  if (ColorPickerID == "menuBackCP")
    menupopup = document.getElementById("formatMenuPopup");
  else if (ColorPickerID == "BackColorPicker")
    menupopup = document.getElementById("BackColorPopup");

  if (menupopup) menupopup.closePopup();

  EditorSetBackgroundColor(color);
  contentWindow.focus();
}

function EditorRemoveBackColor(ColorWellID)
{
  if (ColorWellID)
  {
    var menupopup = document.getElementById("BackColorPopup");
    if (menupopup) menupopup.closePopup();
  }
  //TODO: Set colorwell to browser's default color
  editorShell.SetBackgroundColor("");
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
  editorShell.RemoveTextProperty("href", "");
  contentWindow.focus();
}

/*TODO: We need an oncreate hook to do enabling/disabling for the 
        Format menu. There should be code like this for the 
        object-specific "Properties..." item
*/

// For property dialogs, we want the selected element,
//  but will accept a parent table cell if inside one
function GetSelectedElementOrParentCell()
{
  var element = editorShell.GetSelectedElement("");
  if (!element)
    element = editorShell.GetElementOrParentByTagName("td",null);

  return element;
}

function EditorObjectProperties()
{
  var element = GetSelectedElementOrParentCell();
  dump("EditorObjectProperties: element="+element+"\n");
  if (element)
  {
    dump("TagName="+element.nodeName+"\n");
    switch (element.nodeName)
    {
      case 'IMG':
        EditorInsertOrEditImage();
        break;
      case 'HR':
        EditorInsertOrEditHLine();
        break;
      case 'TABLE':
        EditorInsertOrEditTable(false);
        break;
      case 'TD':
        EditorTableCellProperties();
        break;
      case 'A':
        if(element.href)
          EditorInsertOrEditLink();  
        else if (element.name)
          EditorInsertOrEditNamedAnchor();  
        break;
    }
  } else {
    // We get a partially-selected link if asked for specifically
    element = editorShell.GetSelectedElement("href");
    if (element)
      EditorInsertOrEditLink();  
  }
}

function EditorListProperties()
{
  window.openDialog("chrome://editor/content/EdListProps.xul","_blank", "chrome,close,titlebar,modal");
  contentWindow.focus();
}

function EditorPageProperties()
{
  window.openDialog("chrome://editor/content/EdPageProps.xul","_blank", "chrome,close,titlebar,modal", "");
  contentWindow.focus();
}

function EditorColorProperties()
{
  window.openDialog("chrome://editor/content/EdColorProps.xul","_blank", "chrome,close,titlebar,modal", "");
  contentWindow.focus();
}

// --------------------------- Dialogs ---------------------------

function EditorInsertHTML()
{
  // Resizing doesn't work!
  window.openDialog("chrome://editor/content/EdInsSrc.xul","_blank", "chrome,close,titlebar,modal,resizeable=yes");
  contentWindow.focus();
}

function EditorInsertOrEditLink()
{
  window.openDialog("chrome://editor/content/EdLinkProps.xul","_blank", "chrome,close,titlebar,modal");
  contentWindow.focus();
}

function EditorInsertOrEditImage()
{
  window.openDialog("chrome://editor/content/EdImageProps.xul","_blank", "chrome,close,titlebar,modal");
  contentWindow.focus();
}

function EditorInsertOrEditHLine()
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

function EditorInsertOrEditNamedAnchor()
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

function EditorMakeOrChangeList(listType)
{
  // check the observer node,
  // which is the appropriate button
  var theButton = document.getElementById(listType + "Button");

  if (theButton)
  {
    var buttonFormat = theButton.getAttribute("format");
    var isOn = (listType == buttonFormat);

    if (isOn == 1)
    {
      editorShell.RemoveList(listType);
    }
    else
    {
      editorShell.MakeOrChangeList(listType);
    }

    contentWindow.focus();
  }
  else
  {
    dump("No button found for the " + listType + " style\n");
  }
}

function EditorAlign(align)
{
  editorShell.Align(align);
  contentWindow.focus();
}

function EditorSetDisplayMode(mode)
{
  EditorDisplayMode = mode;
  
  // Editor does the style sheet loading/unloading
  editorShell.SetDisplayMode(mode);

  // Set the UI states

  // TODO: Should we NOT have the menu items and eliminate this?
  var showMenu = document.getElementById("ShowExtraMarkup");
  var hideMenu = document.getElementById("HideExtraMarkup");
  switch (mode ) {
    case 0:
      showMenu.setAttribute("hidden","true");
      hideMenu.removeAttribute("hidden");
      break;

    default: // = 1
      showMenu.removeAttribute("hidden");
      hideMenu.setAttribute("hidden","true");
      break;
  }

  document.getElementById("WYSIWYGModeButton").setAttribute("selected",Number(mode == 0));
  document.getElementById("NormalModeButton").setAttribute("selected",Number(mode == 1));
  document.getElementById("TagModeButton").setAttribute("selected",Number(mode == 2));
  contentWindow.focus();
}

function EditorToggleParagraphMarks()
{
  var menuItem = document.getElementById("viewParagraphMarks");
  if (menuItem)
  {
    var checked = menuItem.getAttribute("checked");
    try {
      editorShell.DisplayParagraphMarks(checked != "true");
    }
    catch(e)
    {
      dump("Failed to load style sheet for paragraph marks\n");
      return;
    }
    if (checked)
      menuItem.removeAttribute("checked");
    else
      menuItem.setAttribute("checked", "true");
  }
}

function EditorPreview()
{
  if (!editorShell.CheckAndSaveDocument(editorShell.GetString("BeforePreview")))
    return;

  fileurl = "";
  try {
    fileurl = window.content.location;
  } catch (e) {
    return;
  }

  // CheckAndSave doesn't tell us if the user said "Don't Save",
  // so make sure we have a url:
  if (fileurl != "" && fileurl != "about:blank")
  {
    window.openDialog("chrome://navigator/content/navigator.xul",
                      "EditorPreview",
                      "chrome,all,dialog=no",
                      fileurl );
  }
}

function EditorPrintSetup()
{
  // Old code? Browser no longer is doing this
  //window.openDialog("resource:/res/samples/printsetup.html", "_blank", "chrome,close,titlebar", "");
  _EditorNotImplemented();
  contentWindow.focus();
}

function EditorPrintPreview()
{
  _EditorNotImplemented();
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
    }
    catch(ex) {
      dump("*** Exception error: StartSpellChecking\n");
      return;
    }
    if( firstMisspelledWord == "")
    {
      try {
        spellChecker.CloseSpellChecking();
      }
      catch(ex) {
        dump("*** Exception error: CloseSpellChecking\n");
        return;
      }
      // No misspelled word - tell user
      editorShell.AlertWithTitle(editorShell.GetString("CheckSpelling"), editorShell.GetString("NoMisspelledWord")); 
    } else {
      // Set spellChecker variable on window
      window.spellChecker = spellChecker;
      try {
        window.openDialog("chrome://editor/content/EdSpellCheck.xul", "_blank", "chrome,close,titlebar,modal", "", firstMisspelledWord);
      }
      catch(ex) {
        dump("*** Exception error: SpellChecker Dialog Closing\n");
        return;
      }
    }
  }
  contentWindow.focus();
}

function SetBackColorString(xulElementID)
{
  var xulElement = document.getElementById(xulElementID);
  if (xulElement)
  {
    var textVal;
    var selectedCountObj = new Object();
    var tagNameObj = new Object();
    var element = editorShell.GetSelectedOrParentTableElement(tagNameObj, selectedCountObj);  

    if (tagNameObj.value == "table")
      textVal = editorShell.GetString("TableBackColor");
    else if (tagNameObj.value == "td")
      textVal = editorShell.GetString("CellBackColor");
    else
      textVal = editorShell.GetString("PageBackColor");

    xulElement.setAttribute("value",textVal);
  }
}

function InitBackColorPopup()
{
  SetBackColorString("BackColorCaption"); 
}

function EditorInitEditMenu()
{
  // We will be modifying the Paste menuitem in the future  
}

function EditorInitFormatMenu()
{
  // Set the string for the background color menu item
  SetBackColorString("backgroundColorMenu"); 

  var menuItem = document.getElementById("objectProperties");
  if (menuItem)
  {
    var element = GetSelectedElementOrParentCell();
    var menuStr = editorShell.GetString("ObjectProperties");
    if (element && element.nodeName)
    {
      var objStr = "";
      menuItem.removeAttribute("disabled");
      switch (element.nodeName)
      {
        case 'IMG':
          objStr = editorShell.GetString("Image");
          break;
        case 'HR':
          objStr = editorShell.GetString("HLine");
          break;
        case 'TABLE':
          objStr = editorShell.GetString("Table");
          break;
        case 'TD':
          objStr = editorShell.GetString("TableCell");
          break;
        case 'A':
          if(element.href)
            objStr = editorShell.GetString("Link");
          else if (element.name)
            objStr = editorShell.GetString("NamedAnchor");
          break;
      }
      menuStr = menuStr.replace(/%obj%/,objStr);        
    }
    else
    {
      // We show generic "Properties" string, but disable menu item
      menuItem.setAttribute("disabled","true");
      // Replace placeholder with "", then remaining space on left side
      menuStr = menuStr.replace(/%obj%/,"").replace(/^\s+/, "");

    }
    menuItem.setAttribute("value", menuStr)
  }
}

function EditorInitToolbars()
{
  // Nothing to do now, but we might want some state updating here
}

function EditorSetDefaultPrefs()
{
  /* only set defaults for new documents */
  var url = document.getElementById("args").getAttribute("value");
  if ( url != "about:blank" )
    return;

  var domdoc;
  try { domdoc = window.editorShell.editorDocument; } catch (e) { dump( e + "\n"); }
  if ( !domdoc )
  {
    dump("EditorSetDefaultPrefs: EDITOR DOCUMENT NOT FOUND\n");
    return;
  }

  // doctype
  var newdoctype = domdoc.implementation.createDocumentType("html", "-//W3C//DTD HTML 4.01 Transitional//EN",
               "");
  if (!domdoc.doctype)
  {
    domdoc.insertBefore(newdoctype, domdoc.firstChild);
  }
  else
    dump("doctype not added");
  
  // try to get preferences service
  var prefs = null;
  try {
    prefs = Components.classes['component://netscape/preferences'];
    prefs = prefs.getService();
    prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
  }
  catch (ex) {
    dump("failed to get prefs service!\n");
    prefs = null;
  }

  // search for head; we'll need this for meta tag additions
  var headelement = 0;
  var headnodelist = domdoc.getElementsByTagName("head");
  if (headnodelist)
  {
    var sz = headnodelist.length;
    if ( sz >= 1 )
    {
      headelement = headnodelist.item(0);
    }
  }
  
  // search for author meta tag.
  // if one is found, don't do anything.
  // if not, create one and make it a child of the head tag 
  //   and set its content attribute to the value of the editor.author preference.
  
  var nodelist = domdoc.getElementsByTagName("meta");
  if ( nodelist )
  {
    var node = 0;
    var listlength = nodelist.length;

    // let's start by assuming we have an author in case we don't have the pref
    var authorFound = false;
    for (var i = 0; i < listlength && !authorFound; i++)
    {
      node = nodelist.item(i);
      if ( node )
      {
        var value = node.getAttribute("name");
        if (value == "author")
        {
          authorFound = true;
        }
      }
    }

    var prefAuthorString = 0;
    try
    {
      prefAuthorString = prefs.CopyCharPref("editor.author");
    }
    catch (ex) {}
    if ( prefAuthorString && prefAuthorString != 0)
    {
      if ( !authorFound && headelement)
      {
        /* create meta tag with 2 attributes */
        var element = domdoc.createElement("meta");
        if ( element )
        {
          AddAttrToElem(domdoc, "name", "Author", element);
          AddAttrToElem(domdoc, "content", prefAuthorString, element);
          headelement.appendChild( element );
        }
      }
    }
    
    // grab charset pref and make it the default charset
    var prefCharsetString = 0;
    try
    {
      prefCharsetString = prefs.CopyCharPref("intl.charset.default");
    }
    catch (ex) {}
    if ( prefCharsetString && prefCharsetString != 0)
    {
        var element = domdoc.createElement("meta");
        if ( element )
        {
          AddAttrToElem(domdoc, "http-equiv", "content-type", element);
          AddAttrToElem(domdoc, "content", "text/html; charset=" + prefCharsetString, element);
          headelement.appendChild( element );
        }
    }

  }

  // color prefs
  var use_custom_colors = false;
  try {
    use_custom_colors = prefs.GetBoolPref("editor.use_custom_colors");
    dump("pref use_custom_colors:" + use_custom_colors + "\n");
  }
  catch (ex) {
    dump("problem getting use_custom_colors as bool, hmmm, still confused about its identity?!\n");
  }

  if ( use_custom_colors )
  {
    // find body node
    var bodyelement = null;
    var bodynodelist = null;
    try {
      bodynodelist = domdoc.getElementsByTagName("body");
      bodyelement = bodynodelist.item(0);
    }
    catch (ex) {
      dump("no body tag found?!\n");
      //  better have one, how can we blow things up here?
    }

    // try to get the default color values.  ignore them if we don't have them.
    var text_color = link_color = active_link_color = followed_link_color = background_color = "";

    try { text_color = prefs.CopyCharPref("editor.text_color"); } catch (e) {}
    try { link_color = prefs.CopyCharPref("editor.link_color"); } catch (e) {}
    try { active_link_color = prefs.CopyCharPref("editor.active_link_color"); } catch (e) {}
    try { followed_link_color = prefs.CopyCharPref("editor.followed_link_color"); } catch (e) {}
    try { background_color = prefs.CopyCharPref("editor.background_color"); } catch(e) {}

    // add the color attributes to the body tag.
    // FIXME:  use the check boxes for each color somehow..
    if (text_color != "")
      AddAttrToElem(domdoc, "text", text_color, bodyelement);
    if (link_color != "")
      AddAttrToElem(domdoc, "link", link_color, bodyelement);
    if (active_link_color != "") 
      AddAttrToElem(domdoc, "alink", active_link_color, bodyelement);
    if (followed_link_color != "")
      AddAttrToElem(domdoc, "vlink", followed_link_color, bodyelement);
    if (background_color != "")
      AddAttrToElem(domdoc, "bgcolor", background_color, bodyelement);
  }

  // auto-save???
}

function AddAttrToElem(dom, attr_name, attr_value, elem)
{
  var a = dom.createAttribute(attr_name);
  if ( a )
  {
    a.value = attr_value;
    elem.setAttributeNode(a);
  }
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

// --------------------------- Logging stuff ---------------------------

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

function EditorExit()
{
  dump("Exiting in EditorExit\n");
  // This is in globalOverlay.js
  goQuitApplication();
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

/* onDirtyChange() is not called */
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

function getColorAndSetColorWell(ColorPickerID, ColorWellID)
{
  var colorWell;
  if (ColorWellID)
    colorWell = document.getElementById(ColorWellID);

  var colorPicker = document.getElementById(ColorPickerID);
  if (colorPicker) 
  {
    // Extract color from colorPicker and assign to colorWell.
    var color = colorPicker.getAttribute('color');
    dump("setColor to: "+color+"\n");

    if (colorWell && color)
    {
      // Use setAttribute so colorwell can be a XUL element, such as titledbutton
      colorWell.setAttribute("style", "background-color: " + color); 
    }
  }
  //TODO: Trigger UI update to get color from the current caret/selection  
  return color;
}

// Call this with insertAllowed = true to allow inserting if not in existing table,
//   else use false to do nothing if not in a table
function EditorInsertOrEditTable(insertAllowed)
{
  var table = editorShell.GetElementOrParentByTagName("table", null);
  if (table) {
    // Edit properties of existing table
    dump("Existing table found ... Editing its properties\n");

    window.openDialog("chrome://editor/content/EdTableProps.xul", "_blank", "chrome,close,titlebar,modal", "","TablePanel");
    contentWindow.focus();
  } else if(insertAllowed) {
    EditorInsertTable();
  }
}

function EditorTableCellProperties()
{
dump("*** EditorTableCellProperties\n");
  var cell = editorShell.GetElementOrParentByTagName("td", null);
  if (cell) {
    // Start Table Properties dialog on the "Cell" panel
    window.openDialog("chrome://editor/content/EdTableProps.xul", "_blank", "chrome,close,titlebar,modal", "", "CellPanel");
    contentWindow.focus();
  }
}

function EditorSelectTableCell()
{
  editorShell.SelectTableCell();
  contentWindow.focus();
}

function EditorSelectAllTableCells()
{
  editorShell.SelectAllTableCells();
  contentWindow.focus();
}

function EditorSelectTable()
{
  editorShell.SelectTableCell();
  contentWindow.focus();
}

function EditorSelectTableRow()
{
  editorShell.SelectTableRow();
  contentWindow.focus();
}

function EditorSelectTableColumn()
{
  editorShell.SelectTableColumn();
  contentWindow.focus();
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

function EditorInsertTableRow(below)
{
dump("EditorInsertTableRow, below="+below+"\n");
  editorShell.InsertTableRow(1,below);
  contentWindow.focus();
}

function EditorInsertTableColumn(after)
{
dump("EditorInsertTableColumn, after="+after+"\n");
  editorShell.InsertTableColumn(1,after);
  contentWindow.focus();
}

function EditorNormalizeTable()
{
  editorShell.NormalizeTable(null);
  contentWindow.focus();
}

function EditorJoinTableCells()
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
  // TODO: Get the number of rows to delete from the selection
  editorShell.DeleteTableRow(1);
  contentWindow.focus();
}

function EditorDeleteTableColumn()
{
  // TODO: Get the number of cols to delete from the selection
  editorShell.DeleteTableColumn(1);
  contentWindow.focus();
}


function EditorDeleteTableCell()
{
  // TODO: Get the number of cells to delete from the selection
  editorShell.DeleteTableCell(1);
  contentWindow.focus();
}

function EditorDeleteTableCellContents()
{
  // TODO: Get the number of cells to delete from the selection
  editorShell.DeleteTableCellContents();
  contentWindow.focus();
}

