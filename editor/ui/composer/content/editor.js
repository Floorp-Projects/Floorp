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
 *    Ryan Cassin (rcassin@supernova.org)
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
var PreviousNonSourceDisplayMode = 1;
var gEditorDisplayMode = 1;  // Normal Editor mode
var EditModeType = "";
var WebCompose = false;     // Set true for Web Composer, leave false for Messenger Composer
var docWasModified = false;  // Check if clean document, if clean then unload when user "Opens"
var gContentWindow = 0;
var gSourceContentWindow = 0;
var gContentWindowDeck;
var gFormatToolbar;
var gEditModeBar;
// Bummer! Can't get at enums from nsIDocumentEncoder.h
var gOutputSelectionOnly = 1;
var gOutputFormatted     = 2;
var gOutputNoDoctype     = 4;
var gOutputBodyOnly      = 8;
var gOutputPreformatted  = 16;
var gOutputWrap          = 32;
var gOutputFormatFlowed  = 64;
var gOutputAbsoluteLinks = 128;
var gOutputEncodeEntities = 256;
var gEditModeLabel;
var gNormalModeButton;
var gTagModeButton;
var gSourceModeButton;
var gPreviewModeButton;
var gIsWindows;
var gIsMac;
var gIsUNIX;
var gIsHTMLEditor = false;

var gPrefs;
// These must be kept in synch with the XUL <options> lists
var gFontSizeNames = new Array("xx-small","x-small","small","medium","large","x-large","xx-large");

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function EditorOnLoad()
{
    // See if argument was passed.
    if ( window.arguments && window.arguments[0] ) {
        // Opened via window.openDialog with URL as argument.    
        // Put argument where EditorStartup expects it.
        document.getElementById( "args" ).setAttribute( "value", window.arguments[0] );
    }
    
    WebCompose = true;
    window.tryToClose = EditorCanClose;

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
    DoRecentFilesMenuSave();  // Save the recent files menu
	 
    // udpate menu items now that we have an editor to play with
    dump("Updating 'create' commands\n");
    window._content.focus();

    window.updateCommands("create");
    BuildRecentMenu();
  },
  
  NotifyDocumentWillBeDestroyed: function() {},
  NotifyDocumentStateChanged:function( isNowDirty )
  {
    /* Notify our dirty detector so this window won't be closed if
       another document is opened */
    if (isNowDirty)
      docWasModified = true;
    
    // hack! Should not need this updateCommands, but there is some controller
    //  bug that this works around. ??
    // comment out the following line because it cause 41573 IME problem on Mac
    //window._content.focus();
    window.updateCommands("create");
    window.updateCommands("save");
  }
};

function EditorStartup(editorType, editorElement)
{
  gIsHTMLEditor = (editorType == "html");

  if (gIsHTMLEditor)
  {
    gSourceContentWindow = document.getElementById("content-source");

    gEditModeBar       = document.getElementById("EditModeToolbar");
    gEditModeLabel     = document.getElementById("EditModeLabel"); 
    gNormalModeButton  = document.getElementById("NormalModeButton");
    gTagModeButton     = document.getElementById("TagModeButton");
    gSourceModeButton  = document.getElementById("SourceModeButton");
    gPreviewModeButton = document.getElementById("PreviewModeButton");

    // The "type" attribute persists, so use that value
    //  to setup edit mode buttons
    ToggleEditModeType(gNormalModeButton.getAttribute("type"));

    // XUL elements we use when switching from normal editor to edit source
    gContentWindowDeck = document.getElementById("ContentWindowDeck");
    gFormatToolbar = document.getElementById("FormatToolbar");
  }

  // store the editor shell in the window, so that child windows can get to it.
  editorShell = editorElement.editorShell;        // this pattern exposes a JS/XBL bug that causes leaks
//  editorShell = editorElement.boxObject.QueryInterface(Components.interfaces.nsIEditorBoxObject).editorShell;
  
  editorShell.Init();
  editorShell.SetEditorType(editorType);

  editorShell.webShellWindow = window;
  editorShell.contentWindow = window._content;

  // add a listener to be called when document is really done loading
  editorShell.RegisterDocumentStateListener( DocumentStateListener );

  // Startup also used by other editor users, such as Message Composer 
  EditorSharedStartup()

  // set up our global prefs object
  GetPrefsService();
   
  // Get url for editor content and load it.
  // the editor gets instantiated by the editor shell when the URL has finished loading.
  var url = document.getElementById("args").getAttribute("value");
  editorShell.LoadUrl(url);
}

// This is the only method also called by Message Composer
function EditorSharedStartup()
{
  // set up JS-implemented commands
  SetupControllerCommands();

  // Just for convenience
  gContentWindow = window._content;

  gIsWin = navigator.appVersion.indexOf("Win") != -1;
  gIsUNIX = (navigator.appVersion.indexOf("X11") || 
             navigator.appVersion.indexOf("nux")) != -1;
  gIsMac = !gIsWin && !gIsUNIX;
  //dump("IsWin="+gIsWin+", IsUNIX="+gIsUNIX+", IsMac="+gIsMac+"\n");

  // Set platform-specific hints for how to select cells
  // Mac uses "Cmd", all others use "Ctrl"
  var tableKey = GetString(gIsMac ? "XulKeyMac" : "TableSelectKey");
  var DragStr = tableKey+GetString("Drag");
  var ClickStr = tableKey+GetString("Click");

  var DelStr = GetString(gIsMac ? "Clear" : "Del");

  document.getElementById("menu_SelectCell").setAttribute("acceltext", ClickStr);
  document.getElementById("menu_SelectRow").setAttribute("acceltext", DragStr);
  document.getElementById("menu_SelectColumn").setAttribute("acceltext", DragStr);
  document.getElementById("menu_SelectAllCells").setAttribute("acceltext", DragStr);
  // And add "Del" or "Clear"
  document.getElementById("menu_DeleteCellContents").setAttribute("acceltext",DelStr);

  // hide UI that we don't have components for
  HideInapplicableUIElements();
}

function _EditorNotImplemented()
{
  dump("Function not implemented\n");
}

function EditorShutdown()
{
  dump("In EditorShutdown..\n");
  return editorShell.Shutdown();
}

// We use this alot!
function GetString(id)
{
  return editorShell.GetString(id);
}

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

  while ( enumerator.hasMoreElements() )
  {
    var window = windowManagerInterface.convertISupportsToDOMWindow( enumerator.getNext() );
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


// used by openLocation. see openLocation.js for additional notes.
function delayedOpenWindow(chrome, flags, url)
{
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
  // but should we try to do it all here or just use nsIFilePicker in EditorShell code?
  editorShell.saveDocument(doSaveAs, doSaveCopy);
}

// Check for changes to document and allow saving before closing
// This is hooked up to the OS's window close widget (e.g., "X" for Windows)
function EditorCanClose()
{
  // Returns FALSE only if user cancels save action
  dump("Calling EditorCanClose\n");
  return editorShell.CheckAndSaveDocument(GetString("BeforeClosing"));
}

// --------------------------- View menu ---------------------------

function EditorViewSource()
{
  // Temporary hack: save to a file and call up the source view window
  // using the local file url.
  if (!editorShell.CheckAndSaveDocument(GetString("BeforeViewSource")))
    return;

  fileurl = "";
  try {
    fileurl = window._content.location;
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
  if(editorShell &&
     (! editorShell.documentModified) &&
       editorShell.editorDocument.location != "about:blank")
  {
    dump(aCharset);
    editorShell.SetDocumentCharacterSet(aCharset);
    editorShell.LoadUrl(editorShell.editorDocument.location);
  }
}


// --------------------------- Text style ---------------------------

function EditorSetTextProperty(property, attribute, value)
{
  editorShell.SetTextProperty(property, attribute, value);
  gContentWindow.focus();
}

function onParagraphFormatChange(paraMenuList, commandID)
{
  var commandNode = document.getElementById(commandID);
  var state = commandNode.getAttribute("state");
  var menuList = document.getElementById("ParagraphSelect");
  if (!menuList) return;

  dump("Updating paragraph format with " + state + "\n");
  
  // force match with "normal"
  if (state == "body")
    state = "";
  
  if (state == "mixed")
  {
    //Selection is the "mixed" ( > 1 style) state
    paraMenuList.selectedItem = null;
    paraMenuList.setAttribute("value",GetString('Mixed'));
  }
  else
  {
    var menuPopup = document.getElementById("ParagraphPopup");
    var menuItems = menuPopup.childNodes;
    for (i=0; i < menuItems.length; i++)
    {
      var menuItem = menuItems.item(i);
      if (menuItem.data == state)
      {
        paraMenuList.selectedItem = menuItem;
        break;
      }
    }
  }
}

function EditorSetParagraphFormat(commandID, paraFormat)
{
  var commandNode = document.getElementById(commandID);
  commandNode.setAttribute("state", paraFormat);
  window._content.focus();   // needed for command dispatch to work
  goDoCommand(commandID);
}

function onChangeColor(colorWell, commandID)
{
  // Get the color from the command node state
  var commandNode = document.getElementById(commandID);
  var color = commandNode.getAttribute("state");
  //dump("onChangeColor -- Color is: "+color+"\n");

  // Use setAttribute so colorwell can be a XUL element, such as titledbutton
  colorWell.setAttribute("style", "background-color: " + color); 
}

function onFontFaceChange(fontFaceMenuList, commandID)
{
  var commandNode = document.getElementById(commandID);
  var state = commandNode.getAttribute("state");

  dump("Updating font face with " + state + "\n");
  
  if (state == "mixed")
  {
    //Selection is the "mixed" ( > 1 style) state
    fontFaceMenuList.selectedItem = null;
    fontFaceMenuList.setAttribute("value",GetString('Mixed'));
  }
  else
  {
    var menuPopup = document.getElementById("FontFacePopup");
    var menuItems = menuPopup.childNodes;
    for (i=0; i < menuItems.length; i++)
    {
      var menuItem = menuItems.item(i);
      if (menuItem.getAttribute("value") && (menuItem.data.toLowerCase() == state.toLowerCase()))
      {
        fontFaceMenuList.selectedItem = menuItem;
        break;
      }
    }
  }
}

function EditorSetFontFace(commandID, fontFace)
{
  dump("Setting font face to " + fontFace + "\n");
  var commandNode = document.getElementById(commandID);
  commandNode.setAttribute("state", fontFace);
  window._content.focus();   // needed for command dispatch to work
  goDoCommand(commandID);
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

function onFontSizeChange(fontSizeMenulist, commandID)
{
  // If we don't match anything, set to "0 (normal)"
  var newIndex = 2;
	var size = fontSizeMenulist.getAttribute("size");
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
  if (fontSizeMenulist.selectedIndex != newIndex)
    fontSizeMenulist.selectedIndex = newIndex;
}

function EditorSetFontSize(size)
{
  if( size == "0" || size == "normal" || 
      size == "medium" )
  {
    editorShell.RemoveTextProperty("font", "size");
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
  gContentWindow.focus();
}

function EditorSelectTextColor(ColorPickerID, ColorWellID)
{
  var color = getColorAndSetColorWell(ColorPickerID, ColorWellID);
  //dump("EditorSelectTextColor: "+color+"\n");

  // Close appropriate menupopup  
  var menupopup;
  if (ColorPickerID == "menuTextCP")
    menupopup = document.getElementById("formatMenuPopup");
  else if (ColorPickerID == "TextColorPicker")
    menupopup = document.getElementById("TextColorPopup");

  if (menupopup) menupopup.closePopup();

  EditorSetFontColor(color);
  gContentWindow.focus();
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
  gContentWindow.focus();
}

function EditorSelectBackColor(ColorPickerID, ColorWellID)
{
  var color = getColorAndSetColorWell(ColorPickerID, ColorWellID);
  //dump("EditorSelectBackColor: "+color+"\n");

  // Close appropriate menupopup  
  var menupopup;
  if (ColorPickerID == "menuBackCP")
    menupopup = document.getElementById("formatMenuPopup");
  else if (ColorPickerID == "BackColorPicker")
    menupopup = document.getElementById("BackColorPopup");

  if (menupopup) menupopup.closePopup();

  EditorSetBackgroundColor(color);
  window._content.focus();
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
  window._content.focus();
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
  window._content.focus();
}

function EditorSetBackgroundColor(color)
{
  editorShell.SetBackgroundColor(color);
  window._content.focus();
}

function EditorApplyStyle(tagName)
{
  editorShell.SetTextProperty(tagName, "", "");
  window._content.focus();
}

function EditorRemoveLinks()
{
  editorShell.RemoveTextProperty("href", "");
  window._content.focus();
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

// --------------------------- Dialogs ---------------------------

function EditorAlign(commandID, alignType)
{
  var commandNode = document.getElementById(commandID);
  // dump("Saving para format state " + alignType + "\n");
  commandNode.setAttribute("state", alignType);
  goDoCommand(commandID);
}

function SetEditMode(mode)
{
  if (gIsHTMLEditor)
  {
    // Be sure toolbar is visible
    gEditModeBar.setAttribute("hidden", "");
    gEditModeBar.setAttribute("collapsed", "");
    // Remember the state
    document.persist("EditModeToolbar","hidden");
    document.persist("EditModeToolbar","collapsed");

    var bodyNode = editorShell.editorDocument.getElementsByTagName("body").item(0);
    if (!bodyNode)
    {
      dump("SetEditMode: We don't have a body node!\n");
      return;
    }
    // Switch the UI mode before inserting contents
    //   so user can't type in source window while new window is being filled
    var previousMode = gEditorDisplayMode;
    if (!SetDisplayMode(mode))
      return;

    if (mode == DisplayModeSource)
    {
      // Get the current contents and output into the SourceWindow
      if (bodyNode)
      {
        var childCount = bodyNode.childNodes.length;
        if( childCount)
        {
          gSourceContentWindow.setAttribute("value",editorShell.GetContentsAs("text/html", gOutputBodyOnly));
          gSourceContentWindow.focus();
          setTimeout("gSourceContentWindow.focus()", 10);
          return;
        }
      }
      // If we fall through, revert to previous node
      SetDisplayMode(PreviousNonSourceDisplayMode);
    }
    else if (previousMode == DisplayModeSource) 
    {
      // We are comming from edit source mode,
      //   so transfer that back into the document
      editorShell.SelectAll();
      editorShell.InsertSource(gSourceContentWindow.value);
      // Clear out the source editor buffer
      gSourceContentWindow.value = "";
      // reset selection to top of doc (wish we could preserve it!)
      if (bodyNode)
        editorShell.editorSelection.collapse(bodyNode, 0);

      window._content.focus();
      // yuck. what is this?
      setTimeout("window._content.focus()", 10);
    }
  }
}

function CancelSourceEditing()
{
  // Empty the source window
  gSourceContentWindow.value="";
  SetDisplayMode(PreviousNonSourceDisplayMode);
}

function SetDisplayMode(mode)
{
  if (gIsHTMLEditor)
  {
    // Already in requested mode:
    //  return false to indicate we didn't switch
    if (mode == gEditorDisplayMode)
      return false;

    gEditorDisplayMode = mode;

    // Save the last non-source mode so we can cancel source editing easily
    if (mode != DisplayModeSource)
      PreviousNonSourceDisplayMode = mode;
  
  
    // Editorshell does the style sheet loading/unloading
    editorShell.SetDisplayMode(mode);

    // Set the UI states
    gPreviewModeButton.setAttribute("selected",Number(mode == DisplayModePreview));
    gNormalModeButton.setAttribute("selected",Number(mode == DisplayModeNormal));
    gTagModeButton.setAttribute("selected",Number(mode == DisplayModeAllTags));
    gSourceModeButton.setAttribute("selected", Number(mode == DisplayModeSource));

    if (mode == DisplayModeSource)
    {
      // Switch to the sourceWindow (second in the deck)
      gContentWindowDeck.setAttribute("index","1");

      // TODO: WE MUST DISABLE APPROPRIATE COMMANDS
      // and change UI to appropriate

      // THIS DOESN'T WORK!
      gSourceContentWindow.focus();
    }
    else 
    {
      // Switch to the normal editor (first in the deck)
      gContentWindowDeck.setAttribute("index","0");

      // TODO: WE MUST ENABLE APPROPRIATE COMMANDS
      // and change UI back to "normal"

      window._content.focus();
    }
    return true;
  }
}

function ToggleEditModeType()
{
  if (gIsHTMLEditor)
  {
    if (EditModeType == "text")
    {
      EditModeType = "image";
      gNormalModeButton.setAttribute("value","");
      gTagModeButton.setAttribute("value","");
      gSourceModeButton.setAttribute("value","");
      gPreviewModeButton.setAttribute("value","");
      // Advanced users don't need to see the label (cleaner look)
      gEditModeLabel.setAttribute("hidden","true");
    }
    else
    {
      EditModeType = "text";
      gNormalModeButton.setAttribute("value","Normal");
      gTagModeButton.setAttribute("value","Show All Tags");
      gSourceModeButton.setAttribute("value","HTML Source");
      gPreviewModeButton.setAttribute("value","Edit Preview");
      gEditModeLabel.setAttribute("hidden","");
    }

    gNormalModeButton.setAttribute("type",EditModeType);
    gTagModeButton.setAttribute("type",EditModeType);
    gSourceModeButton.setAttribute("type",EditModeType);
    gPreviewModeButton.setAttribute("type",EditModeType);

    // Remember the state
    document.persist("NormalModeButton","type");
  }
}

function EditorToggleParagraphMarks()
{
  var menuItem = document.getElementById("viewParagraphMarks");
  if (menuItem)
  {
    // Note that the 'type="checbox"' mechanism automatically
    //  toggles the "checked" state before the oncommand is called,
    //  so if "checked" is true now, it was just switched to that mode
    var checked = menuItem.getAttribute("checked");
    try {
      editorShell.DisplayParagraphMarks(checked == "true");
    }
    catch(e)
    {
      dump("Failed to load style sheet for paragraph marks\n");
      return;
    }
  }
}

function EditorInitEditMenu()
{
  var DelStr = GetString(gIsMac ? "Clear" : "Delete");

  // Yuck. We should be doing this at build time, using
  // platform-specific overlays or dtd files.
  
  // Change menu text to "Clear" for Mac 
  // TODO: Should this be in globalOverlay.j?
  document.getElementById("menu_delete").setAttribute("value",DelStr);
  // TODO: WHAT IS THE Mac STRING?
  if (!gIsMac)
    document.getElementById("menu_delete").setAttribute("acceltext", GetString("Del"));
    
  //TODO: We should modify the Paste menuitem to build a submenu 
  //      with multiple paste format types
}

function EditorOpenUrl(url)
{
  if (!url || url.length == 0)
    return; 

  // if the existing window is untouched, just load there
  if (!FindAndSelectEditorWindowWithURL(url))
  {
    if (PageIsEmptyAndUntouched())
    {
      window.editorShell.LoadUrl(url);
    }
    else
    {
  	  // open new window
      window.openDialog("chrome://editor/content",
  	                    "_blank",
  	                    "chrome,dialog=no,all",
  	                    url);
    }
  }
}

function DoRecentFilesMenuSave()
{
  // Can't do anything if no prefs
  if (!gPrefs) return;

  var curTitle = window.editorShell.editorDocument.title;
  var curUrl = window.editorShell.editorDocument.location;
  var newDoc = (curUrl == "about:blank");

  if(!newDoc) // Can't preform this function if document is new
  {
    // Always put latest-opened URL at start of array
    ShuffleRecentMenu(curUrl)
    SaveRecentFilesPrefs(curTitle, curUrl, "0");
  }
}

function ShuffleRecentMenu(curUrl)
{
  // This function simply saves the remaining items (from entry 2 and beyond) to the prefs file.
  var historyCount = 10;  // This will be changed by the next line, but if they don't have that pref, this is a good default
  try { historyCount = gPrefs.CopyUnicharPref("editor.history.url_maximum"); } catch(e) {} // Number of items in recent files menu
  var titleArray = new Array(historyCount);
  var urlArray = new Array(historyCount);
  
  for (i = 0; i < historyCount; i++)
  {
    titleArray[i] = getUnicharPref("editor.history_title_"+i);
    urlArray[i] = getUnicharPref("editor.history_url_"+i);
  }

  var placeholder = 1; // i+1, holds number that decides which menuitem the recent file is going in
  for (i = 0; i < historyCount; i++)
  {
    // If we skip over an item in the array, placeholder is not incremented
    if (urlArray[i] != curUrl)
    {
      // Save the menu one spot down in the list
      SaveRecentFilesPrefs(titleArray[i], urlArray[i], placeholder);
      placeholder++;
    }
  }
  gPrefs.SavePrefFile(); // Save the prefs file
}

function SaveRecentFilesPrefs(title, url, i)
{
  if (!url || url.length == 0) {
    return;
  }
    
  // Now save the title and url of the document recently opened to the array
  setUnicharPref("editor.history_title_"+i, title);
  setUnicharPref("editor.history_url_"+i, url);
}

function BuildRecentMenu()
{
  // Populate the Recent Files Menu.
  // Can't do anything if we don't have any prefs
  if(!gPrefs) return;
  // Build the submenu
  var popup = document.getElementById("menupopup_RecentFiles");
  if (!popup) return;
  // Delete existing menu
  while (popup.firstChild)
    popup.removeChild(popup.firstChild);

  // Again, this is changed in the next line but if the pref never existed, this default coincides with above
  var historyCount = 10; 
  try { historyCount = gPrefs.CopyUnicharPref("editor.history.url_maximum"); } catch(e) {}

  for(i = 0; i < historyCount; i++)
  {
    var title = getUnicharPref("editor.history_title_"+i);
	 var url = getUnicharPref("editor.history_url_"+i);

	 if (!url || url.length == 0)
	   break;
	 
	 AppendRecentMenuitem(i+1, popup, title, url, i+2);
  }
}

function AppendRecentMenuitem(accessKey, menupopup, title, url, menuIndex)
{
  if (menupopup)
  {
    menuItem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "menuitem");
    if (menuItem)
    {
      // Build the menuitem using the title (or url) and accesskey
      var itemString;
      if (accessKey > 9) 
      {
        // Access key is two digits so spaces are put in where the access key
        // would go and the title is added.
        itemString = "   "+title;  // Set title and spaces (no access key)
        if (title.length < 1)
          // There is no title to display on the menuitem so use the URL instead
          itemString = "    "+url;
	  } else {
        itemString = accessKey+" "+title;  // Set the menuitem to use the title and accesskey
        if (title.length < 1)
          // There is no title to display on the menuitem so use the URL instead
          itemString = accessKey+" "+url;
        menuItem.setAttribute("accesskey", accessKey);
      }

      menuItem.setAttribute("value", itemString);
      menuItem.setAttribute("data", url);
      menuItem.setAttribute("oncommand", "EditorOpenUrl(getAttribute('data'))");
      menupopup.appendChild(menuItem);
    }
  }
}

function setUnicharPref(aPrefName, aPrefValue)
{
  if (!gPrefs) return "";
  try
  {
    gPrefs.SetUnicharPref(aPrefName, aPrefValue);
  }
  catch(e)
  {
  }
}

function  getUnicharPref(aPrefName, aDefVal)
{
  if (!gPrefs) return "";
  try
  {
    return gPrefs.CopyUnicharPref(aPrefName);
  }
  catch(e)
  {
    return "";
  }
}

function EditorInitFormatMenu()
{
  // Set the string for the background color menu item
  SetBackColorString("backgroundColorMenu"); 

  // Set strings and enable for the [Object] Properties item
  // Note that we directly do the enabling instead of
  //  using goSetCommandEnabled since we already have the menuitem
  var menuItem = document.getElementById("objectProperties");
  if (menuItem)
  {
    var element = GetSelectedElementOrParentCell();
    var menuStr = GetString("ObjectProperties");
    if (element && element.nodeName)
    {
      var objStr = "";
      menuItem.removeAttribute("disabled");
      var name = element.nodeName.toLowerCase();
      switch (name)
      {
        case 'img':
          objStr = GetString("Image");
          break;
        case 'hr':
          objStr = GetString("HLine");
          break;
        case 'table':
          objStr = GetString("Table");
          break;
        case 'td':
          objStr = GetString("TableCell");
          break;
        case 'a':
          if (element.name)
            objStr = GetString("NamedAnchor");
          else if(element.href)
            objStr = GetString("Link");
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
    menuItem.setAttribute("value", menuStr);
  }
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
      textVal = GetString("TableBackColor");
    else if (tagNameObj.value == "td")
      textVal = GetString("CellBackColor");
    else
      textVal = GetString("PageBackColor");

    xulElement.setAttribute("value",textVal);
  }
}

function InitBackColorPopup()
{
  SetBackColorString("BackColorCaption"); 
}

function InitParagraphMenu()
{
  var mixedObj = new Object();
  var state = editorShell.GetParagraphState(mixedObj);
  //dump("InitParagraphMenu: state="+state+"\n");
  var IDSuffix = "normal";

  // PROBLEM: When we get blockquote, it masks other styles contained by it
  // We need a separate method to get blockquote state  
  if (state.length > 0)
  {
    if (state.charAt(0) == "h")
    {
      // We have a heading style - remove any checkmark in this submenu
      //  by first setting the first item, then removing the check
      document.getElementById("menu_normal").setAttribute("checked", "true");
      document.getElementById("menu_normal").removeAttribute("checked");
      return;
    }
    IDSuffix = state;
  }

  document.getElementById("menu_"+IDSuffix).setAttribute("checked", "true");
}

function InitHeadingMenu()
{
  var mixedObj = new Object();
  var state = editorShell.GetParagraphState(mixedObj);
  //dump("InitHeadingMenu: state="+state+"\n");
  var IDSuffix = "noHeading";

  if (state.length > 0 && state.charAt(0) == "h")
    IDSuffix = state;

  document.getElementById("menu_"+IDSuffix).setAttribute("checked", "true");
}

function InitListMenu()
{
  var mixedObj = new Object();
  var state = editorShell.GetListState(mixedObj);
  //dump("InitListMenu: state="+state+"\n");
  var IDSuffix = "noList";
  
  if (state.length > 0)
  {
    if (state == "dl")
      state = editorShell.GetListItemState(mixedObj);

    if (state.length > 0)
      IDSuffix = state;
  }

  document.getElementById("menu_"+IDSuffix).setAttribute("checked", "true");
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
  var newdoctype = domdoc.implementation.createDocumentType("html", "-//W3C//DTD HTML 4.01 Transitional//EN","");
  if (!domdoc.doctype)
  {
    domdoc.insertBefore(newdoctype, domdoc.firstChild);
  }
  else
  {
    domdoc.replaceChild(newdoctype, domdoc.doctype);
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
// dump(" === onButtonUpdate called\n");
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

var gHaveSpellChecker = false;
var gSoughtSpellChecker = false;

//-----------------------------------------------------------------------------------
function IsSpellCheckerInstalled()
{
  if (gSoughtSpellChecker)
    return gHaveSpellChecker;

  var spellcheckerClass = Components.classes["mozilla.spellchecker.1"]; 
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

    // Spelling item is in its own menu block, so remove it
    // (we can't use "hidden" since class="hide-in-IM" CSS rule overrides!)
    var spellingSepItem  = document.getElementById("sep_checkspelling");
    if (spellingSepItem)
      spellingSepItem.parentNode.removeChild(spellingSepItem);
  }

}

function onEditorFocus()
{
dump("onEditorFocus called\n");
}

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

// Command Updating Strategy:
//   Don't update on on selection change, only when menu is displayed,
//   with this "oncreate" hander:
function EditorInitTableMenu()
{
  // Change text on the "Join..." item depending if we
  //   are joining selected cells or just cell to right
  // TODO: What to do about normal selection that crosses
  //       table border? Try to figure out all cells
  //       included in the selection?
  var menuText;

  // Use "Join selected cells if there's more than 1 cell selected
  var tagNameObj = new Object;
  var countObj = new Object;
  if (window.editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj) && countObj.value > 1)
    menuText = GetString("JoinSelectedCells");
  else
    menuText = GetString("JoinCellToRight");

  document.getElementById("menu_tableJoinCells").setAttribute("value",menuText);

  // Set enable states for all table commands
  goUpdateTableMenuItems(document.getElementById("composerTableMenuItems"));
}

function goUpdateTableMenuItems(commandset)
{
  var enabled = false; 

  var enabledIfTable = false;
  if (window.editorShell && window.editorShell.documentEditable)
  {
    var selectedCountObj = new Object();
    var tagNameObj = new Object();
    var element = editorShell.GetSelectedOrParentTableElement(tagNameObj, selectedCountObj);  
    if (element)
    {
      // Value when we need to have a selected table or inside a table
      enabledIfTable = true;  

      // All others require being inside a cell or selected cell
      enabled = (tagNameObj.value == "td");
    }
  }

  // Loop through command nodes
  for (var i = 0; i < commandset.childNodes.length; i++)
  {
    var commandID = commandset.childNodes[i].getAttribute("id");
    if (commandID)
    {
      if (commandID == "cmd_InsertTable" ||
          commandID == "cmd_tableJoinCells" ||
          commandID == "cmd_tableSplitCell")
      {
        // Call the update method in the command class
        goUpdateCommand(commandID);
      } 
      // Directly set with the values calculated here
      else if (commandID == "cmd_DeleteTable" ||
               commandID == "cmd_NormalizeTable")
      {
        goSetCommandEnabled(commandID, enabledIfTable);
      } else {
        goSetCommandEnabled(commandID, enabled);
      }
    }
  }
}

//-----------------------------------------------------------------------------------
// Helpers for inserting and editing tables:

function IsInTable()
{
  return (window.editorShell && window.editorShell.documentEditable &&
          null != window.editorShell.GetElementOrParentByTagName("table", null));
}

function IsInTableCell()
{
  return (window.editorShell && window.editorShell.documentEditable &&
          null != window.editorShell.GetElementOrParentByTagName("td", null));
}

function IsSelectionInOneCell()
{
  var selection = window.editorShell.editorSelection;

  if (selection && selection.rangeCount == 1)
  {
    // We have a "normal" single-range selection
    if (!selection.isCollapsed &&
       selection.anchorNode != selection.focusNode)
    {
      // Check if both nodes are within the same cell
      var anchorCell = window.editorShell.GetElementOrParentByTagName("td", selection.anchorNode);
      var focusCell = window.editorShell.GetElementOrParentByTagName("td", selection.focusNode);
      return (focusCell != null && anchorCell != null && (focusCell == anchorCell));
    }
    // Collapsed selection or anchor == focus (thus must be in 1 cell)
    return true;
  }
  return false;
}

// Call this with insertAllowed = true to allow inserting if not in existing table,
//   else use false to do nothing if not in a table
function EditorInsertOrEditTable(insertAllowed)
{
  if (IsInTable()) {
    // Edit properties of existing table
    window.openDialog("chrome://editor/content/EdTableProps.xul", "_blank", "chrome,close,titlebar,modal", "","TablePanel");
    window._content.focus();
  } else if(insertAllowed) {
    EditorInsertTable();
  }
}

function EditorInsertTable()
{
dump("EditorInsertTable\n");
  // Insert a new table
  window.openDialog("chrome://editor/content/EdInsertTable.xul", "_blank", "chrome,close,titlebar,modal", "");
  window._content.focus();
}

function EditorTableCellProperties()
{
  var cell = editorShell.GetElementOrParentByTagName("td", null);
  if (cell) {
    // Start Table Properties dialog on the "Cell" panel
    window.openDialog("chrome://editor/content/EdTableProps.xul", "_blank", "chrome,close,titlebar,modal", "", "CellPanel");
    window._content.focus();
  }
}

