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
 *    Ryan Cassin (kidteckco@hotmail.com)
 */

/* Main Composer window UI control */

var editorShell;
var documentModified;
var prefAuthorString = "";
var NormalMode = 1;
var PreviewMode = 2;
// These must match enums in nsIEditorShell.idl:
var DisplayModePreview = 0;
var DisplayModeNormal = 1;
var DisplayModeAllTags = 2;
var DisplayModeSource = 3;
var EditorDisplayMode = 1;  // Normal Editor mode
var WebCompose = false;     // Set true for Web Composer, leave false for Messenger Composer
var docWasModified = false;  // Check if clean document, if clean then unload when user "Opens"
var contentWindow = 0;
var sourceContentWindow = 0;

var gPrefs;
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
dump("EditorOnLoad: url to load="+window.arguments[0]+"\n");
        document.getElementById( "args" ).setAttribute( "value", window.arguments[0] );
    }
    
    WebCompose = true;
    
    // Continue with normal startup.
    EditorStartup('html', document.getElementById("content-frame"));
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

function PageIsEmptyAndUntouched()
{
  return (editorShell != null) && (editorShell.documentIsEmpty == true) && (docWasModified == false);
}

// This is called when the real editor document is created,
// before it's loaded.
var DocumentStateListener =
{
  NotifyDocumentCreated: function() 
  {
    // Call EditorSetDefaultPrefs first so it gets the default author before initing toolbars
    EditorSetDefaultPrefs();
    EditorInitToolbars();
    
    // udpate menu items now that we have an editor to play with
    //dump("Updating 'create' commands\n");
    content.focus();
    window.updateCommands("create");
  },
  
  NotifyDocumentWillBeDestroyed: function() {},
  NotifyDocumentStateChanged:function( isNowDirty )
  {
    /* Notify our dirty detector so this window won't be closed if
       another document is opened */
    if (isNowDirty)
      docWasModified = true;
    
    // hack! Should not need this, but there is some controller bug that this
    // works around.
    window.updateCommands("create");
  }
};

function EditorStartup(editorType, editorElement)
{
  contentWindow = window.content;
  sourceContentWindow = document.getElementById("HTMLSourceWindow");
  
  // store the editor shell in the window, so that child windows can get to it.
  editorShell = editorElement.editorShell;
  
  editorShell.Init();
  editorShell.SetEditorType(editorType);

  editorShell.webShellWindow = window;
  editorShell.contentWindow = contentWindow;

  // hide UI that we don't have components for
  HideInapplicableUIElements();

  // set up JS-implemented commands
  SetupControllerCommands();
  
  // add a listener to be called when document is really done loading
  editorShell.RegisterDocumentStateListener( DocumentStateListener );
 
  // set up our global prefs object
  GetPrefsService();
   
  // Get url for editor content and load it.
  // the editor gets instantiated by the editor shell when the URL has finished loading.
  var url = document.getElementById("args").getAttribute("value");
dump("EditorStartup: url="+url+"\n");
  editorShell.LoadUrl(url);
}


function _EditorNotImplemented()
{
  dump("Function not implemented\n");
}

function _EditorObsolete()
{
  dump("Function is obsolete\n");
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
  return _EditorObsolete();
}

function EditorOpenRemote()
{
  return _EditorObsolete();
}

// used by openLocation. see openLocation.js for additional notes.
function delayedOpenWindow(chrome, flags, url)
{
dump("delayedOpenWindow: URL="+url+"\n");
  if (PageIsEmptyAndUntouched())
    editorShell.LoadUrl(url);
  else
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
  return _EditorObsolete();
}

function EditorSaveAs()
{
  return _EditorObsolete();
}

function EditorPrint()
{
  return _EditorObsolete();
}

function EditorPrintSetup()
{
  return _EditorObsolete();
}

function EditorPrintPreview()
{
  return _EditorObsolete();
}

function EditorClose()
{
  return _EditorObsolete();
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
  return _EditorObsolete();
}

function EditorFindNext()
{
  return _EditorObsolete();
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

function onParagraphFormatChange(commandID)
{
  var commandNode = document.getElementById(commmandID);
  var state = commandNode.getAttribute("state");
dump(" ==== onParagraphFormatChange was called. state="+state+"|\n");

  return; //TODO: REWRITE THIS
  var menulist = document.getElementById("ParagraphSelect");
  if (menulist)
  { 
    // If we don't match anything, set to "normal"
    var newIndex = 0;
  	var format = select.getAttribute("format");
    if ( format == "mixed")
    {
//dump("Mixed paragraph format *******\n");
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
    if (menulist.selectedIndex != newIndex)
      menulist.selectedIndex = newIndex;
  }
}

function EditorSetParagraphFormat(commandID, paraFormat)
{
  // editorShell.SetParagraphFormat(paraFormat);
  var commandNode = document.getElementById(commandID);
  dump("Saving para format state " + paraFormat + "\n");
  commandNode.setAttribute("state", paraFormat);
  window.content.focus();   // needed for command dispatch to work
  goDoCommand(commandID);
}


function onFontFaceChange()
{
  return; //TODO: REWRITE THIS
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
  if (fontFace == "tt") {
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
//dump("Setting focus to content window...\n");
  contentWindow.focus();
}

function EditorSelectFontSize()
{
  var select = document.getElementById("FontSizeSelect");
//dump("EditorSelectFontSize: "+gFontSizeNames[select.selectedIndex]+"\n");
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
  return _EditorObsolete();
}

function EditorDecreaseFontSize()
{
  return _EditorObsolete();
}

function EditorSelectTextColor(ColorPickerID, ColorWellID)
{
  var color = getColorAndSetColorWell(ColorPickerID, ColorWellID);
// dump("EditorSelectTextColor: "+color+"\n");

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


function SetManualTextColor()
{
}

function SetManualTextColor()
{
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
  return _EditorObsolete();
}

function EditorToggleStyle(styleName)
{
  return _EditorObsolete();
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
  // dump("EditorObjectProperties: element="+element+"\n");
  if (element)
  {
    // dump("TagName="+element.nodeName+"\n");
    switch (element.nodeName)
    {
      case 'IMG':
        goDoCommand("cmd_image");
        break;
      case 'HR':
        goDoCommand("cmd_hline");
        break;
      case 'TABLE':
        EditorInsertOrEditTable(false);
        break;
      case 'TD':
        EditorTableCellProperties();
        break;
      case 'A':
        if(element.href)
          goDoCommand("cmd_link");
        else if (element.name)
          goDoCommand("cmd_anchor");
        break;
    }
  } else {
    // We get a partially-selected link if asked for specifically
    element = editorShell.GetSelectedElement("href");
    if (element)
      goDoCommand("cmd_link");
  }
}

function EditorListProperties()
{
  return _EditorObsolete();
}

function EditorPageProperties()
{
  return _EditorObsolete();
}

function EditorColorProperties()
{
  return _EditorObsolete();
}

// --------------------------- Dialogs ---------------------------

function EditorInsertHTML()
{
  return _EditorObsolete();
}

function EditorInsertOrEditLink()
{
  return _EditorObsolete();
}

function EditorInsertOrEditImage()
{
  return _EditorObsolete();
}

function EditorInsertOrEditHLine()
{
 return _EditorObsolete();
}

function EditorInsertChars()
{
  return _EditorObsolete();
}

function EditorInsertOrEditNamedAnchor()
{
  return _EditorObsolete();
}

function EditorIndent(indent)
{
  return _EditorObsolete();
}

function EditorMakeOrChangeList(listType)
{
  return _EditorObsolete();
}

function EditorAlign(commandID, alignType)
{
  var commandNode = document.getElementById(commandID);
  // dump("Saving para format state " + alignType + "\n");
  commandNode.setAttribute("state", alignType);
  goDoCommand(commandID);
}

function EditorSetDisplayMode(mode)
{
  EditorDisplayMode = mode;
  
  // Editor does the style sheet loading/unloading
  editorShell.SetDisplayMode(mode);

  // Set the UI states
  document.getElementById("PreviewModeButton").setAttribute("selected",Number(mode == DisplayModePreview));
  document.getElementById("NormalModeButton").setAttribute("selected",Number(mode == DisplayModeNormal));
  document.getElementById("TagModeButton").setAttribute("selected",Number(mode == DisplayModeAllTags));
  var HTMLButton = document.getElementById("SourceModeButton");
  if (HTMLButton)
    HTMLButton.setAttribute("selected", Number(mode == DisplayModeSource));
dump(sourceContentWindow+"=SourceWindow\n");  
  if (mode == DisplayModeSource)
    sourceContentWindow.focus();
  else 
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
  return _EditorObsolete();
}

function CheckSpelling()
{
  return _EditorObsolete();
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
      prefAuthorString = gPrefs.CopyCharPref("editor.author");
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
      prefCharsetString = gPrefs.CopyCharPref("intl.charset.default");
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
    use_custom_colors = gPrefs.GetBoolPref("editor.use_custom_colors");
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

    try { text_color = gPrefs.CopyCharPref("editor.text_color"); } catch (e) {}
    try { link_color = gPrefs.CopyCharPref("editor.link_color"); } catch (e) {}
    try { active_link_color = gPrefs.CopyCharPref("editor.active_link_color"); } catch (e) {}
    try { followed_link_color = gPrefs.CopyCharPref("editor.followed_link_color"); } catch (e) {}
    try { background_color = gPrefs.CopyCharPref("editor.background_color"); } catch(e) {}

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
  return _EditorObsolete();
}


//--------------------------------------------------------------------
function initFontStyleMenu(menuPopup)
{
  for (var i = 0; i < menuPopup.childNodes.length; i++)
  {
    var menuItem = menuPopup.childNodes[i];
    var theStyle = menuItem.getAttribute("state");
    if (theStyle)
    {
      menuItem.setAttribute("checked", theStyle);
    }
  }
}

//--------------------------------------------------------------------
function onButtonUpdate(button, commmandID)
{
dump(" === onButtonUpdate called\n");
  var commandNode = document.getElementById(commmandID);
  var state = commandNode.getAttribute("state");

  button.setAttribute("toggled", state);
}


// --------------------------- Status calls ---------------------------
function onStyleChange(theStyle)
{
  dump("in onStyleChange with " + theStyle + "\n");
  
  var broadcaster = document.getElementById("cmd_" + theStyle);
  var isOn = broadcaster.getAttribute("state");

  // PrintObject(broadcaster);

  var theButton = document.getElementById(theStyle + "Button");
  if (theButton)
  {
    // dump("setting button to " + isOn + "\n");
    theButton.setAttribute("toggled", (isOn == "true") ? 1 : 0);
  }

  var theMenuItem = document.getElementById(theStyle + "MenuItem");
  if (theMenuItem)
  {
    // dump("setting menuitem to " + isOn + "\n");
    theMenuItem.setAttribute("checked", isOn);
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
	  var isOn = theButton.getAttribute(listType);
  	if (isOn == "true") {
      theButton.setAttribute("toggled", "true");
    } else {
      theButton.setAttribute("toggled", "false");
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

var gHaveSpellChecker = false;
var gSoughtSpellChecker = false;

//-----------------------------------------------------------------------------------
function IsSpellCheckerInstalled()
{
  return true;      // fix me when kin checks in

  if (gSoughtSpellChecker)
    return gHaveSpellChecker;

  var spellcheckerClass = Components.classes["org.mozilla.spellchecker.1"]; 
  gHaveSpellChecker = (spellcheckerClass != null);
  gSoughtSpellChecker = true;
  return gHaveSpellChecker;
}

//-----------------------------------------------------------------------------------
function HideInapplicableUIElements()
{
  // if no spell checker, remove spell checker ui
  if (!IsSpellCheckerInstalled())
  {
    dump("Hiding spell checker items\n");
    
    var spellingButton = document.getElementById("spellingButton");
    if (spellingButton)
      spellingButton.setAttribute("hidden", "true");
    
    var spellingMenuItem = document.getElementById("menu_checkspelling");
    if (spellingMenuItem)
      spellingMenuItem.setAttribute("hidden", "true");
  }

}


//-----------------------------------------------------------------------------------
function GetPrefsService()
{
  // Store the prefs object
  try {
    var prefsService = Components.classes['component://netscape/preferences'];
    if (prefsService) 
      prefsService = prefsService.getService();
    if (prefsService)
      gPrefs = prefsService.QueryInterface(Components.interfaces.nsIPref);    

    if (!gPrefs)
      dump("failed to get prefs service!\n");
  }
  catch(ex)
  {
	  dump("failed to get prefs service!\n");
  }
}
