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
var WebCompose = false;     // Set true for Web Composer, leave false for Messenger Composer
var docWasModified = false;  // Check if clean document, if clean then unload when user "Opens"
var gContentWindow = 0;
var gSourceContentWindow = 0;
var gContentWindowDeck;
var gFormatToolbar;
var gFormatToolbarHidden = false;
var gFormatToolbarCollapsed;
var gEditModeBar;
// Bummer! Can't get at enums from nsIDocumentEncoder.h
var gOutputSelectionOnly = 1;
var gOutputFormatted     = 2;
//var gOutputNoDoctype     = 4;  // OutputNoDoctype has been obsoleted
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
var gColorObj = new Object();
var gPrefs;
var gDefaultTextColor = "";
var gDefaultBackgroundColor = "";

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
	 
    BuildRecentMenu();
    window._content.focus();

    // udpate menu items now that we have an editor to play with
    // Note: This must be AFTER window._content.focus();
    window.updateCommands("create");
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

    // XUL elements we use when switching from normal editor to edit source
    gContentWindowDeck = document.getElementById("ContentWindowDeck");
    gFormatToolbar = document.getElementById("FormatToolbar");
  }

  // store the editor shell in the window, so that child windows can get to it.
  editorShell = editorElement.editorShell;        // this pattern exposes a JS/XBL bug that causes leaks
  
  editorShell.Init();
  editorShell.editorType = editorType;

  editorShell.webShellWindow = window;
  editorShell.contentWindow = window._content;

  // add a listener to be called when document is really done loading
  editorShell.RegisterDocumentStateListener( DocumentStateListener );

  // set up our global prefs object
  GetPrefsService();
 
  // Startup also used by other editor users, such as Message Composer 
  EditorSharedStartup();

  // Commands specific to the Composer Application window,
  //  (i.e., not embeded editors) 
  //  such as file-related commands, HTML Source editing, Edit Modes...
  SetupComposerWindowCommands();

  // Get url for editor content and load it.
  // the editor gets instantiated by the editor shell when the URL has finished loading.
  var url = document.getElementById("args").getAttribute("value");
  editorShell.LoadUrl(url);
}

// This is the only method also called by Message Composer
function EditorSharedStartup()
{
  // set up JS-implemented commands for HTML editing
  SetupHTMLEditorCommands();

  // Just for convenience
  gContentWindow = window._content;

  gIsWindows = navigator.appVersion.indexOf("Win") != -1;
  gIsUNIX = (navigator.appVersion.indexOf("X11") || 
             navigator.appVersion.indexOf("nux")) != -1;
  gIsMac = !gIsWindows && !gIsUNIX;
  //dump("IsWin="+gIsWindows+", IsUNIX="+gIsUNIX+", IsMac="+gIsMac+"\n");

  // Set platform-specific hints for how to select cells
  // Mac uses "Cmd", all others use "Ctrl"
  var tableKey = GetString(gIsMac ? "XulKeyMac" : "TableSelectKey");
  var dragStr = tableKey+GetString("Drag");
  var clickStr = tableKey+GetString("Click");

  var delStr = GetString(gIsMac ? "Clear" : "Del");

  SafeSetAttribute("menu_SelectCell", "acceltext", clickStr);
  SafeSetAttribute("menu_SelectRow", "acceltext", dragStr);
  SafeSetAttribute("menu_SelectColumn", "acceltext", dragStr);
  SafeSetAttribute("menu_SelectAllCells", "acceltext", dragStr);
  // And add "Del" or "Clear"
  SafeSetAttribute("menu_DeleteCellContents", "acceltext", delStr);

  // Remove a Privacy menu that causes problems
  // (method is in tasksOverlay.js)
  HideImage();

  // hide UI that we don't have components for
  RemoveInapplicableUIElements();

  // Use global prefs if EditorStartup already run, 
  //   else get service for other editor users
  if (!gPrefs) GetPrefsService();
  
  // Use browser colors as initial values for editor's default colors
  var BrowserColors = GetDefaultBrowserColors();
  if (BrowserColors)
  {
    gDefaultTextColor = BrowserColors.TextColor;
    gDefaultBackgroundColor = BrowserColors.BackgroundColor;
  }
//dump(" *** EditorShared startup: gDefaultTextColor="+gDefaultTextColor+", gDefaultBackgroundColor="+gDefaultBackgroundColor+"\n");
}

// Get these to use for initial default text and background,
//  and also pass to prefs and color dialogs
function GetDefaultBrowserColors()
{
  var colors = new Object();
  var useWinColors = false;
  if (gIsWindows)
  {
    // In Windows only, there's a pref to use system colors instead of pref colors
    try { useWinColors = gPrefs.GetBoolPref("browser.display.wfe.use_windows_colors"); } catch (e) {}
  }

  if (!useWinColors)
  {
    try { colors.TextColor = gPrefs.CopyCharPref("browser.display.foreground_color"); } catch (e) {}

    try { colors.BackgroundColor = gPrefs.CopyCharPref("browser.display.background_color"); } catch (e) {}
  }
  // Use OS colors for text and background if explicitly asked or pref is not set
  if (!colors.TextColor)
    colors.TextColor = "windowtext";
  
  if (!colors.BackgroundColor)
    colors.BackgroundColor = "window";

  colors.LinkColor = gPrefs.CopyCharPref("browser.anchor_color");
  colors.VisitedLinkColor = gPrefs.CopyCharPref("browser.visited_color");

  return colors;
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

function SafeSetAttribute(nodeID, attributeName, attributeValue)
{
    var theNode = document.getElementById(nodeID);
    if (theNode)
        theNode.setAttribute(attributeName, attributeValue);
}

// We use this alot!
function GetString(id)
{
  return editorShell.GetString(id);
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

function editorSendPage()
{
  var docModified = editorShell.documentModified;
  var pageUrl = window.editorShell.editorDocument.location;
  if (pageUrl != "about:blank" && !docModified)
  {
    var pageTitle = window.editorShell.editorDocument.title;
    window.openDialog("chrome://messenger/content/messengercompose/messengercompose.xul", "_blank", 
                        "chrome,all,dialog=no", "attachment='" + pageUrl + "',body='" + pageUrl +
                        "',subject='" + pageTitle + "',bodyislink=true");
  } 
  else if (CheckAndSaveDocument(GetString("SendPageReason")), DocumentHasBeenSaved())
    editorSendPage();

  window._content.focus();
}

function DocumentHasBeenSaved()
{
  var fileurl = "";
  try {
    fileurl = window._content.location;
  } catch (e) {
    return false;
  }

  if (fileurl == "" || fileurl == "about:blank")
    return false;

  // We have a file URL already
  return true;
}

function CheckAndSaveDocument(reasonToSave, allowDontSave)
{
  var document = editorShell.editorDocument;
  if (!editorShell.documentModified)
    return true;
  
  var title = window.editorShell.editorDocument.title;
  if (!title)
    title = GetString("untitled");
      
  var dialogTitle = window.editorShell.GetString("SaveDocument");
  var dialogMsg = window.editorShell.GetString("SaveFilePrompt");
  dialogMsg = (dialogMsg.replace(/%title%/,title)).replace(/%reason%/,reasonToSave);
  
  var result = {value:0};
  commonDialogsService.UniversalDialog(
    window,
    null,
    dialogTitle,
    dialogMsg,
    null,
    window.editorShell.GetString("Save"),     // Save Button
    window.editorShell.GetString("Cancel"),   // Cancel Button
    (allowDontSave ? window.editorShell.GetString("DontSave") : null), // Don't Save Button
    null,
    null,
    null,
    {value:0},
    {value:0},
    "chrome://global/skin/question-icon.gif",
    {value:"false"},
    (allowDontSave ? 3 : 2),
    0,
    0,
    result
    );
   
   if (result.value == 0) 
   {
     // Save
     var success = window.editorShell.saveDocument(false, false);
     return success;
   }
   
   if (result.value == 2) // "Don't Save"
     return true;

  // Default or result.value == 1 (Cancel)
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

// Check for changes to document and allow saving before closing
// This is hooked up to the OS's window close widget (e.g., "X" for Windows)
function EditorCanClose()
{
  // Returns FALSE only if user cancels save action

  // "true" means allow "Don't Save" button
  var canClose = CheckAndSaveDocument(GetString("BeforeClosing"), true);

  // This is our only hook into closing via the "X" in the caption
  //   or "Quit" (or other paths?)
  //   so we must shift association to another
  //   editor or close any non-modal windows now
  if (canClose && window.InsertCharWindow)
    SwitchInsertCharToAnotherEditorOrClose();

  return canClose;
}

// --------------------------- View menu ---------------------------

function EditorSetDocumentCharacterSet(aCharset)
{
  if(editorShell) 
  {
    editorShell.SetDocumentCharacterSet(aCharset);
    if( editorShell.editorDocument.location != "about:blank")
    {
      editorShell.LoadUrl(editorShell.editorDocument.location);
    }
  }
}

// ------------------------------------------------------------------
function updateCharsetPopupMenu(menuPopup)
{
  if(editorShell.documentModified)
  {
    for (var i = 0; i < menuPopup.childNodes.length; i++)
    {
      var menuItem = menuPopup.childNodes[i];
		menuItem.setAttribute('disabled', 'true');
    }
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
    for (var i=0; i < menuItems.length; i++)
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

function doStatefulCommand(commandID, newState)
{
  var commandNode = document.getElementById(commandID);
  if (commandNode)
      commandNode.setAttribute("state", newState);
  window._content.focus();   // needed for command dispatch to work
  goDoCommand(commandID);
}

function onFontFaceChange(fontFaceMenuList, commandID)
{
  var commandNode = document.getElementById(commandID);
  var state = commandNode.getAttribute("state");

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
    for (var i=0; i < menuItems.length; i++)
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

function EditorSelectFontSize()
{
  var select = document.getElementById("FontSizeSelect");
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
    for (var i = 0; i < gFontSizeNames.length; i++)
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

function onFontColorChange()
{
  var commandNode = document.getElementById("cmd_fontColor");
  if (commandNode)
  {
    var color = commandNode.getAttribute("state");
    var button = document.getElementById("TextColorButton");
    if (button)
    {
      // No color set - get color set on page or other defaults
      if (!color)
        color = gDefaultTextColor;

      button.setAttribute("style", "background-color:"+color); 
    }
  }
}

function onBackgroundColorChange()
{
  var commandNode = document.getElementById("cmd_backgroundColor");
  if (commandNode)
  {
    var color = commandNode.getAttribute("state");
    var button = document.getElementById("BackgroundColorButton");
    if (button)
    {
      if (!color)
        color = gDefaultBackgroundColor;

      button.setAttribute("style", "background-color:"+color); 
    }
  }
}

function GetBackgroundElementWithColor()
{
  gColorObj.Type = "";
  gColorObj.PageColor = "";
  gColorObj.TableColor = "";
  gColorObj.CellColor = "";
  gColorObj.BackgroundColor = "";

  var tagNameObj = new Object;
  var countObj = new Object;
  var element = window.editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);
  if (element && tagNameObj && tagNameObj.value)
  {
    gColorObj.BackgroundColor = element.getAttribute("bgcolor");
    if (tagNameObj.value.toLowerCase() == "td")
    {
      gColorObj.Type = "Cell";
      gColorObj.CellColor = gColorObj.BackgroundColor;
      
      // Get any color that might be on parent table
      var table = GetParentTable(element);
      gColorObj.TableColor = table.getAttribute("bgcolor");
    }
    else
    {
      gColorObj.Type = "Table";
      gColorObj.TableColor = gColorObj.BackgroundColor;
    }
  }
  else
  {
    element = GetBodyElement();
    if (element)
    {
      gColorObj.Type = "Page";
      gColorObj.BackgroundColor = element.getAttribute("bgcolor");
      gColorObj.PageColor = gColorObj.BackgroundColor;
    }
  }
  return element;
}

function EditorSelectColor(colorType)
{
  if (!gColorObj)
   gColorObj = new Object;

  var element;
  var table;
  var currentColor = "";
  var commandNode;
  
  if (!colorType)
    colorType = "";

  if (colorType == "Text")
  {
    gColorObj.Type = colorType;

    // Get color from command node state
    commandNode = document.getElementById("cmd_fontColor");
    currentColor = commandNode.getAttribute("state");
    gColorObj.TextColor = currentColor;
  }
  else
  {
    element = GetBackgroundElementWithColor();
    if (!element)
      return;

    // Get the table if we found a cell
    if (gColorObj.Type == "Table")
      table = element;
    else if (gColorObj.Type == "Cell")
      table = GetParentTable(element);

    // Save to avoid resetting if not necessary
    currentColor = gColorObj.BackgroundColor;

    if (colorType == "TableOrCell" || colorType == "Cell")
    {
      if (gColorObj.Type == "Cell")
        gColorObj.Type = colorType;
      else if (gColorObj.Type != "Table")
        return;
    }
    else if (colorType == "Table" && gColorObj.Type == "Page")
      return;
    
    if (colorType == "" && gColorObj.Type == "Cell")
    {
      // Using empty string for requested type means
      //  we can let user select cell or table
      gColorObj.Type = "TableOrCell";
    }
  }
  // Save the type we are really requesting
  colorType = gColorObj.Type;

  // Launch the ColorPicker dialog
  // TODO: Figure out how to position this under the color buttons on the toolbar
  window.openDialog("chrome://editor/content/EdColorPicker.xul", "_blank", "chrome,close,titlebar,modal", "", gColorObj);

  // User canceled the dialog
  if (gColorObj.Cancel)
    return;

  if (colorType == "Text")
  {
    if (currentColor != gColorObj.TextColor)
      window.editorShell.SetTextProperty("font", "color", gColorObj.TextColor);
  
    // Update the command state (this will trigger color button update)
    goUpdateCommand("cmd_fontColor");
  }
  else if (element)
  {
    if (gColorObj.Type == "Table")
    {
      // Set background on a table
      // Note that we shouldn't trust "currentColor" because of "TableOrCell" behavior
      if (table)
      {
        var bgcolor = table.getAttribute("bgcolor");
        if (bgcolor != gColorObj.BackgroundColor)
        {
          if (gColorObj.BackgroundColor)
            window.editorShell.SetAttribute(table, "bgcolor", gColorObj.BackgroundColor);
          else
            window.editorShell.RemoveAttribute(table, "bgcolor");
        }
      }
    } 
    else if (currentColor != gColorObj.BackgroundColor)
      window.editorShell.SetBackgroundColor(gColorObj.BackgroundColor);
    
    goUpdateCommand("cmd_backgroundColor");
  }
  window._content.focus();
}

function GetParentTable(element)
{
  var node = element;
  while (node)
  {
    if (node.tagName.toLowerCase() == "table")
      return node;

    node = node.parentNode;
  }
  return node;
}

/*TODO: We need an oncreate hook to do enabling/disabling for the 
        Format menu. There should be code like this for the 
        object-specific "Properties" item
*/

// For property dialogs, we want the selected element,
//  but will accept a parent link, list, or table cell if inside one
function GetObjectForProperties()
{
  var element = editorShell.GetSelectedElement("");
  if (element)
    return element;

  // Find nearest parent of selection anchor node 
  //   that is a link, list, table cell, or table

  var anchorNode = editorShell.editorSelection.anchorNode;
  if (!anchorNode) return null;
  var node;
  if (anchorNode.firstChild)
  {
    // Start at actual selected node
    var offset = editorShell.editorSelection.anchorOffset;
    // Note: If collapsed, offset points to element AFTER caret,
    //  thus node may be null
    node = anchorNode.childNodes.item(offset);
  }
  if (!node)
    node = anchorNode;

  while (node)
  {
    if (node.tagName)
    {
      var tagName = node.tagName.toLowerCase();

      // Done when we hit the body
      if (tagName == "body") break;

      if ((tagName == "a" && node.href) ||
          tagName == "ol" || tagName == "ul" || tagName == "dl" ||
          tagName == "td" || tagName == "th" ||
          tagName == "table")
      {
        return node;
      }
    }
    node = node.parentNode;
  }
  return null;
}

function SetEditMode(mode)
{
  if (gIsHTMLEditor)
  {
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

    var source;

    if (mode == DisplayModeSource)
    {
      // Get the entire document's source string

      var flags = gOutputEncodeEntities;
      // 
      var prettyPrint = gPrefs.GetBoolPref("editor.prettyprint");
      if (prettyPrint)
        flags |= gOutputFormatted;

      source = editorShell.GetContentsAs("text/html", flags);
      var start = source.search(/<html/i);
      if (start == -1) start = 0;
      gSourceContentWindow.value = source.slice(start);
      gSourceContentWindow.focus();
    }
    else if (previousMode == DisplayModeSource) 
    {
      // We are comming from edit source mode,
      //   so transfer that back into the document
      source = gSourceContentWindow.value;
      editorShell.RebuildDocumentFromSource(source);

      // Must handle <title> here to make sure new value is updated
      //  everwhere (also covers bug in RebuildDocumentFromSource that strips it out!)
      var titleStart = source.search(/<title>/i);
      if (titleStart != -1)
      {
        // Skip over tag name
        titleStart += 7;
        var titleEnd = source.indexOf("<",titleStart);
        if (titleEnd > titleStart)
        {
          var title = source.slice(titleStart, titleEnd);
          editorShell.SetDocumentTitle(title);
        }
      }
      // Clear out the string buffers
      source = null;
      gSourceContentWindow.value = null;

      // reset selection to top of doc (wish we could preserve it!)
      if (bodyNode)
        editorShell.editorSelection.collapse(bodyNode, 0);

      window._content.focus();
    }
  }
}

function CancelHTMLSource()
{
  // Don't convert source text back into the DOM document
  gSourceContentWindow.value = "";
  SetDisplayMode(PreviousNonSourceDisplayMode);
}


function FinishHTMLSource()
{
  // Switch edit modes -- converts source back into DOM document
  SetEditMode(PreviousNonSourceDisplayMode);
}

function CollapseItem(id, collapse)
{
  var item = document.getElementById(id);
  if (item)
  {
	  if(collapse != (item.getAttribute("collapsed") == "true"))
		  item.setAttribute("collapsed", collapse ? "true" : "");
  }
}

function DisableItem(id, disable)
{
  var item = document.getElementById(id);
  if (item)
  {
	  if(disable != (item.getAttribute("disabled") == "true"))
		  item.setAttribute("disabled", disable ? "true" : "");
  }
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

      DisableMenusForHTMLSource(true);

      //Hide the formating toolbar if not already hidden
      gFormatToolbarHidden = gFormatToolbar.getAttribute("hidden");
      if (gFormatToolbarHidden != "true")
      {
        gFormatToolbar.setAttribute("hidden", "true");
      }

      gSourceContentWindow.focus();
    }
    else 
    {
      // Switch to the normal editor (first in the deck)
      gContentWindowDeck.setAttribute("index","0");

      // Restore menus and toolbars
      DisableMenusForHTMLSource(false);

      if (gFormatToolbarHidden != "true")
      {
        gFormatToolbar.setAttribute("hidden", gFormatToolbarHidden);
      }

      window._content.focus();
    }

    // We must set check on menu item since toolbar may have been used
    document.getElementById("viewPreviewMode").setAttribute("checked","false");
    document.getElementById("viewNormalMode").setAttribute("checked","false");
    document.getElementById("viewAllTagsMode").setAttribute("checked","false");
    document.getElementById("viewSourceMode").setAttribute("checked","true");

    var menuID;
    switch(mode)
    {
      case DisplayModePreview:
        menuID = "viewPreviewMode";
        break;
      case DisplayModeNormal:
        menuID = "viewNormalMode";
        break;
      case DisplayModeAllTags:
        menuID = "viewAllTagsMode";
        break;
      case DisplayModeSource:
        menuID = "viewSourceMode";
        break;
    }
    if (menuID)
      document.getElementById(menuID).setAttribute("checked","true");
    
    return true;
  }
  return false;
}

// We disable all items in View menu except Edit mode and sidebar items
function DisableMenusForHTMLSource(disable)
{
  // Disable toolbar buttons
  DisableItem("spellingButton", disable);
  DisableItem("imageButton", disable);
  DisableItem("hlineButton", disable);
  DisableItem("tableButton", disable);
  DisableItem("linkButton", disable);
  DisableItem("namedAnchorButton", disable);

  // Any toolbar can be toggled on/off except the format toolbar
  DisableItem("viewFormatToolbar", disable);


  // Top-level menus that we completely hide
  CollapseItem("insertMenu", disable);
  CollapseItem("formatMenu", disable);
  CollapseItem("tableMenu", disable);

  // Edit menu items
  DisableItem("menu_find", disable);
  DisableItem("menu_findnext", disable);
  DisableItem("menu_checkspelling", disable);

  // Disable all items in the view menu except mode switch items
  var viewMenu = document.getElementById("viewMenu");
  // menuitems are children of the menupopup child
  var children = viewMenu.firstChild.childNodes;
  for (var i = 0; i < children.length; i++)
  {
    var item = children.item(i);
    if (item.id != "viewToolbar" &&
        item.id != "viewNormalMode" &&
        item.id != "viewAllTagsMode" &&
        item.id != "viewSourceMode" &&
        item.id != "viewPreviewMode" &&
        item.id != "sidebar-menu")
    {
      DisableItem(item.id, disable);
    }
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
    catch(e) { return; }
  }
}

function EditorInitEditMenu()
{
/*
  // Fake the keybinding hint for the "Clear" item
  // (Don't bother for Mac?)
  if (!gIsMac)
    document.getElementById("menu_clear").setAttribute("acceltext", GetString("Del"));
*/

  //TODO: We should modify the Paste menuitem to build a submenu 
  //      with multiple paste format types
}

function EditorOpenUrl(url)
{
  if (!url)
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
  var i;
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
  if (!url) {
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

  var a=1; // Keeps track of which access key to use in the menuitem
  for (var i = 0; i < historyCount; i++)
  {
    var title = getUnicharPref("editor.history_title_"+i);
    var url = getUnicharPref("editor.history_url_"+i);

    if (!url)
      break;

    // If the current url is already opened, don't put useless entries into the menu
    if (url != window.editorShell.editorDocument.location)
    {
      AppendRecentMenuitem(a, popup, title, url);
      a++;
    }
  }
}

function AppendRecentMenuitem(accessKey, menupopup, title, url)
{
  if (menupopup)
  {
    var menuItem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "menuitem");
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
  if (!gPrefs) return;
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
  }
  return "";
}

function EditorInitFormatMenu()
{
  InitObjectPropertiesMenuitem("objectProperties");
  InitRemoveStylesMenuitems("removeStylesMenuitem", "removeLinksMenuitem");
  // Set alignment check
}

function InitObjectPropertiesMenuitem(id)
{
  // Set strings and enable for the [Object] Properties item
  // Note that we directly do the enabling instead of
  //  using goSetCommandEnabled since we already have the menuitem
  var menuItem = document.getElementById(id);
  if (!menuItem) return null;

  var element = GetObjectForProperties();
  var menuStr = GetString("ObjectProperties");
  var name;

  if (element && element.nodeName)
  {
    var objStr = "";
    menuItem.setAttribute("disabled", "");
    name = element.nodeName.toLowerCase();
    switch (name)
    {
      case "img":
        objStr = GetString("Image");
        break;
      case "hr":
        objStr = GetString("HLine");
        break;
      case "table":
        objStr = GetString("Table");
        break;
      case "th":
        name = "td";
      case "td":
        objStr = GetString("TableCell");
        break;
      case "ol":
      case "ul":
      case "dl":
        objStr = GetString("List");
        break;
      case "a":
        if (element.name)
        {
          objStr = GetString("NamedAnchor");
          name = "anchor";
        }
        else if(element.href)
        {
          objStr = GetString("Link");
          name = "href";
        }
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
  menuItem.setAttribute("accesskey",GetString("ObjectPropertiesAccessKey"));
  return name;
}

function InitParagraphMenu()
{
  var mixedObj = new Object();
  var state = editorShell.GetParagraphState(mixedObj);
  var IDSuffix;

  // PROBLEM: When we get blockquote, it masks other styles contained by it
  // We need a separate method to get blockquote state  

  // We use "x" as uninitialized paragraph state
  if (!state || state == "x")
    IDSuffix = "bodyText" // No paragraph container
  else
    IDSuffix = state;
  
  // Set "radio" check on one item, but...
  var menuItem = document.getElementById("menu_"+IDSuffix);
  menuItem.setAttribute("checked", "true");

  // ..."bodyText" is returned if mixed selection, so remove checkmark
  if (mixedObj.value)
    menuItem.setAttribute("checked", "false");
}

function InitListMenu()
{
  var mixedObj = new Object();
  var state = editorShell.GetListState(mixedObj);
  var IDSuffix = "noList";

  if (state)
  {
    if (state == "dl")
      state = editorShell.GetListItemState(mixedObj);

    if (state)
      IDSuffix = state;
  }
  // Set "radio" check on one item, but...
  var menuItem = document.getElementById("menu_"+IDSuffix);
  menuItem.setAttribute("checked", "true");

  // ..."noList" is returned if mixed selection, so remove checkmark
  if (mixedObj.value)
    menuItem.setAttribute("checked", "false");
}

function InitAlignMenu()
{
  var mixedObj = new Object();
  // Note: GetAlignment DOESN'T set the "mixed" flag,
  //  but always returns "left" in mixed state
  // We'll pay attention to it here so it works when fixed
  var state = editorShell.GetAlignment(mixedObj);
  var IDSuffix;

  if (!state)
    IDSuffix = "left"
  else
    IDSuffix = state;
  
  var menuItem = document.getElementById("menu_"+IDSuffix);
  menuItem.setAttribute("checked", "true");

  if (mixedObj.value)
    menuItem.setAttribute("checked", "false");
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

  var element;
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
    // we should do charset first since we need to have charset before
    // hitting other 8-bit char in other meta tags
    // grab charset pref and make it the default charset
    var prefCharsetString = 0;
    try
    {
      prefCharsetString = gPrefs.getLocalizedUnicharPref("intl.charset.default");
    }
    catch (ex) {}
    if ( prefCharsetString && prefCharsetString != 0)
    {
        element = domdoc.createElement("meta");
        if ( element )
        {
          AddAttrToElem(domdoc, "http-equiv", "content-type", element);
          AddAttrToElem(domdoc, "content", "text/html; charset=" + prefCharsetString, element);
          headelement.appendChild( element );
        }
    }

    var node = 0;
    var listlength = nodelist.length;

    // let's start by assuming we have an author in case we don't have the pref
    var authorFound = false;
    for (var i = 0; i < listlength && !authorFound; i++)
    {
      node = nodelist.item(i);
      if ( node )
      {
        var value = node.getAttribute("name").toLowerCase();
        if (value == "author")
        {
          authorFound = true;
        }
      }
    }

    var prefAuthorString = 0;
    try
    {
      prefAuthorString = gPrefs.CopyUnicharPref("editor.author");
    }
    catch (ex) {}
    if ( prefAuthorString && prefAuthorString != 0)
    {
      if ( !authorFound && headelement)
      {
        /* create meta tag with 2 attributes */
        element = domdoc.createElement("meta");
        if ( element )
        {
          AddAttrToElem(domdoc, "name", "author", element);
          AddAttrToElem(domdoc, "content", prefAuthorString, element);
          headelement.appendChild( element );
        }
      }
    }
    
  }

  // Get editor color prefs
  var use_custom_colors = false;
  try {
    use_custom_colors = gPrefs.GetBoolPref("editor.use_custom_colors");
  }
  catch (ex) {}

// XXX BUG: this is always comming up TRUE even when pref panel is used to set false
dump(" *** editor.use_custom_colors="+use_custom_colors+"\n");

  if ( use_custom_colors )
  {
    // find body node
    var bodyelement = GetBodyElement();

    // try to get the default color values.  ignore them if we don't have them.
    var text_color;
    var link_color;
    var active_link_color;
    var followed_link_color;
    var background_color;
    
    try { text_color = gPrefs.CopyCharPref("editor.text_color"); } catch (e) {}
    try { link_color = gPrefs.CopyCharPref("editor.link_color"); } catch (e) {}
    try { active_link_color = gPrefs.CopyCharPref("editor.active_link_color"); } catch (e) {}
    try { followed_link_color = gPrefs.CopyCharPref("editor.followed_link_color"); } catch (e) {}
    try { background_color = gPrefs.CopyCharPref("editor.background_color"); } catch(e) {}

    // add the color attributes to the body tag.
    // and use them for the default text and background colors if not empty
    if (text_color)
    {
      AddAttrToElem(domdoc, "text", text_color, bodyelement);
      gDefaultTextColor = text_color;
    }
    if (background_color)
    {
      AddAttrToElem(domdoc, "bgcolor", background_color, bodyelement);
      gDefaultBackgroundColor = background_color
    }

    if (link_color)
      AddAttrToElem(domdoc, "link", link_color, bodyelement);
    if (active_link_color) 
      AddAttrToElem(domdoc, "alink", active_link_color, bodyelement);
    if (followed_link_color)
      AddAttrToElem(domdoc, "vlink", followed_link_color, bodyelement);
  }


  // auto-save???
}

function GetBodyElement()
{
  try {
    var bodyNodelist = window.editorShell.editorDocument.getElementsByTagName("body");
    if (bodyNodelist)
      return bodyNodelist.item(0);
  }
  catch (ex) {
    dump("no body tag found?!\n");
    //  better have one, how can we blow things up here?
  }
  return null;
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
  }
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
  
  return true;
}

// --------------------------- Logging stuff ---------------------------

function EditorGetNodeFromOffsets(offsets)
{
  var node = null;
  node = editorShell.editorDocument;

  for (var i = 0; i < offsets.length; i++)
  {
    node = node.childNodes[offsets[i]];
  }

  return node;
}

function EditorSetSelectionFromOffsets(selRanges)
{
  var rangeArr, start, end, node, offset;
  var selection = editorShell.editorSelection;

  selection.clearSelection();

  for (var i = 0; i < selRanges.length; i++)
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
  var commandNode = document.getElementById(commmandID);
  var state = commandNode.getAttribute("state");

  button.setAttribute("toggled", state);
}

//--------------------------------------------------------------------
function onStateButtonUpdate(button, commmandID, onState)
{
  var commandNode = document.getElementById(commmandID);
  var state = commandNode.getAttribute("state");

  button.setAttribute("toggled", (state == onState) ? "true" : "false");
}


// --------------------------- Status calls ---------------------------
function onStyleChange(theStyle)
{
  //dump("in onStyleChange with " + theStyle + "\n");
  
  var broadcaster = document.getElementById("cmd_" + theStyle);
  var isOn = broadcaster.getAttribute("state");

  // PrintObject(broadcaster);

  var theButton = document.getElementById(theStyle + "Button");
  if (theButton)
  {
    theButton.setAttribute("toggled", (isOn == "true") ? 1 : 0);
  }

  var theMenuItem = document.getElementById(theStyle + "MenuItem");
  if (theMenuItem)
  {
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

function getColorAndSetColorWell(ColorPickerID, ColorWellID)
{
  var colorWell;
  if (ColorWellID)
    colorWell = document.getElementById(ColorWellID);

  var colorPicker = document.getElementById(ColorPickerID);
  if (colorPicker) 
  {
    // Extract color from colorPicker and assign to colorWell.
    var color = colorPicker.getAttribute("color");

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
  dump("Have SpellChecker = "+gHaveSpellChecker+"\n");
  return gHaveSpellChecker;
}

var gHaveFind = false;
var gSoughtFind = false;
//-----------------------------------------------------------------------------------
function IsFindInstalled()
{
  if (gSoughtFind)
    return gHaveFind;

  var findClass = Components.classes["component://netscape/appshell/component/find"]; 
  gHaveFind = (findClass != null);
  gSoughtFind = true;
  dump("Have Find = "+gHaveFind+"\n");
  return gHaveFind;
}

//-----------------------------------------------------------------------------------
function RemoveInapplicableUIElements()
{
  // For items that are in their own menu block, remove associated separator
  // (we can't use "hidden" since class="hide-in-IM" CSS rule interferes)

   // if no find, remove find ui
  if (!IsFindInstalled())
  {
    var findMenuItem = document.getElementById("menu_find");
    if (findMenuItem)
      findMenuItem.setAttribute("hidden", "true");

    findMenuItem = document.getElementById("menu_findnext");
    if (findMenuItem)
      findMenuItem.setAttribute("hidden", "true");
    
    var findSepItem  = document.getElementById("sep_find");
    if (findSepItem)
      findSepItem.parentNode.removeChild(findSepItem);
  }

   // if no spell checker, remove spell checker ui
  if (!IsSpellCheckerInstalled())
  {
    var spellingButton = document.getElementById("spellingButton");
    if (spellingButton)
      spellingButton.setAttribute("hidden", "true");
    
    var spellingMenuItem = document.getElementById("menu_checkspelling");
    if (spellingMenuItem)
      spellingMenuItem.setAttribute("hidden", "true");

    var spellingSepItem  = document.getElementById("sep_checkspelling");
    if (spellingSepItem)
      spellingSepItem.parentNode.removeChild(spellingSepItem);
  }
}

function GetPrefsService()
{
  // Store the prefs object
  try {
    var prefsService = Components.classes["component://netscape/preferences"];
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
  InitJoinCellMenuitem("menu_JoinTableCells");

  // Set enable states for all table commands
  goUpdateTableMenuItems(document.getElementById("composerTableMenuItems"));
}

function InitJoinCellMenuitem(id)
{
  // Change text on the "Join..." item depending if we
  //   are joining selected cells or just cell to right
  // TODO: What to do about normal selection that crosses
  //       table border? Try to figure out all cells
  //       included in the selection?
  var menuText;
  var menuItem = document.getElementById(id);
  if (!menuItem) return;

  // Use "Join selected cells if there's more than 1 cell selected
  var tagNameObj = new Object;
  var countObj = new Object;
  if (window.editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj) && countObj.value > 1)
    menuText = GetString("JoinSelectedCells");
  else
    menuText = GetString("JoinCellToRight");

  menuItem.setAttribute("value",menuText);
  menuItem.setAttribute("accesskey",GetString("JoinCellAccesskey"));
}

function InitRemoveStylesMenuitems(removeStylesId, removeLinksId)
{
  // Change wording of menuitems depending on selection
  var stylesItem = document.getElementById(removeStylesId);
  var linkItem = document.getElementById(removeLinksId);
  
  var isCollapsed = editorShell.editorSelection.isCollapsed;
  if (stylesItem)
  {
    stylesItem.setAttribute("value", isCollapsed ? GetString("StopTextStyles") : GetString("RemoveTextStyles"));
    stylesItem.setAttribute("accesskey", GetString("RemoveTextStylesAccesskey"));
  }
  if (linkItem)
  {
    linkItem.setAttribute("value", isCollapsed ? GetString("StopLinks") : GetString("RemoveLinks"));
    linkItem.setAttribute("accesskey", GetString("RemoveLinksAccesskey"));
    // Note: disabling text style is a pain since there are so many - forget it!

    // Disable if not in a link, but always allow "Remove" 
    //  if selection isn't collapsed since we only look at anchor node 
    DisableItem(removeLinksId, isCollapsed && !window.editorShell.GetElementOrParentByTagName("href", null));
  }
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
          commandID == "cmd_JoinTableCells" ||
          commandID == "cmd_SplitTableCell")
      {
        // Call the update method in the command class
        goUpdateCommand(commandID);
      } 
      // Directly set with the values calculated here
      else if (commandID == "cmd_DeleteTable" ||
               commandID == "cmd_NormalizeTable" ||
               commandID == "cmd_TableOrCellColor")
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

function EditorOnFocus()
{
  if (window.InsertCharWindow) return;

  // Find window with an InsertCharsWindow and switch association to this one
  var windowWithDialog = FindEditorWithInsertCharDialog();
  if (windowWithDialog)
  {
    // Switch the dialog to current window
    // this sets focus to dialog, so bring focus back to editor window
    if (SwitchInsertCharToThisWindow(windowWithDialog))
      window.focus();
  }
}

function SwitchInsertCharToThisWindow(windowWithDialog)
{
  if (windowWithDialog && windowWithDialog.InsertCharWindow)
  {
	  // Move dialog association to the current window
    window.InsertCharWindow = windowWithDialog.InsertCharWindow;
    windowWithDialog.InsertCharWindow = null;

    // Switch the dialog's editorShell and opener to current window's
    window.InsertCharWindow.editorShell = window.editorShell;
    window.InsertCharWindow.opener = window;

    // Bring dialog to the forground
    window.InsertCharWindow.focus();
    return true;
  }
  return false;
}

function FindEditorWithInsertCharDialog()
{
  // Find window with an InsertCharsWindow and switch association to this one
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
	var enumerator = windowManagerInterface.getEnumerator( null );

	while ( enumerator.hasMoreElements()  )
	{
		var  tempWindow = enumerator.getNext();
		if (tempWindow != window && tempWindow.InsertCharWindow)
		{
      return tempWindow;
		}
	}
  return null;
}

function EditorFindOrCreateInsertCharWindow()
{
  if (window.InsertCharWindow)
    window.InsertCharWindow.focus();
  else
  {
    // Since we switch the dialog during EditorOnFocus(),
    //   this should really never be found, but it's good to be sure
    var windowWithDialog = FindEditorWithInsertCharDialog();
    if (windowWithDialog)
    {
      SwitchInsertCharToThisWindow(windowWithDialog);
    }
    else
    {
      // The dialog will set window.InsertCharWindow to itself
      window.openDialog("chrome://editor/content/EdInsertChars.xul", "_blank", "chrome,close,titlebar", "");
    }
  }
}

// Find another HTML editor window to associate with the InsertChar dialog
//   or close it if none found  (May be a mail composer)
function SwitchInsertCharToAnotherEditorOrClose()
{
	if (window.InsertCharWindow)
  {
    var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	  var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
	  var enumerator = windowManagerInterface.getEnumerator( null );

    // TODO: Fix this to search for command controllers and look for "cmd_InsertChars"
    // For now, detect just Web Composer and HTML Mail Composer
	  while ( enumerator.hasMoreElements()  )
	  {
		  var  tempWindow = enumerator.getNext();
		  if (tempWindow != window && tempWindow.editorShell &&
          tempWindow.gIsHTMLEditor || tempWindow.editorShell.editorType == "htmlmail")
//          tempWindow.editorShell.editorDocument.commandDispatcher.getControllerForCommand("cmd_InsertChars"))
		  {
        tempWindow.InsertCharWindow = window.InsertCharWindow;
        window.InsertCharWindow = null;

        tempWindow.InsertCharWindow.editorShell = tempWindow.editorShell;
        tempWindow.InsertCharWindow.opener = tempWindow;
        return;
		  }
	  }
    // Didn't find another editor - close the dialog
    window.InsertCharWindow.close();
  }
}
