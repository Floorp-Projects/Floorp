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
 *    Simon Fraser (sfraser@netscape.com)
 */

/* Implementations of nsIControllerCommand for composer commands */


var gComposerCommandManager = null;
var commonDialogsService = Components.classes["component://netscape/appshell/commonDialogs"].getService();
commonDialogsService = commonDialogsService.QueryInterface(Components.interfaces.nsICommonDialogs);

//-----------------------------------------------------------------------------------
function SetupControllerCommands()
{
  gComposerCommandManager = GetComposerController();
  if (!gComposerCommandManager)
    return;
  
  dump("Registering commands\n");
  
  gComposerCommandManager.registerCommand("cmd_newEditor",  nsNewEditorCommand);

  gComposerCommandManager.registerCommand("cmd_open",       nsOpenCommand);
  gComposerCommandManager.registerCommand("cmd_saveAsCharset",       nsSaveAsCharsetCommand);
  gComposerCommandManager.registerCommand("cmd_revert",     nsRevertCommand);
  gComposerCommandManager.registerCommand("cmd_openRemote", nsOpenRemoteCommand);
  gComposerCommandManager.registerCommand("cmd_preview",    nsPreviewCommand);
  gComposerCommandManager.registerCommand("cmd_quit",       nsQuitCommand);

  gComposerCommandManager.registerCommand("cmd_find",       nsFindCommand);
  gComposerCommandManager.registerCommand("cmd_findNext",   nsFindNextCommand);
  gComposerCommandManager.registerCommand("cmd_spelling",   nsSpellingCommand);

  gComposerCommandManager.registerCommand("cmd_editHTML",    nsEditHTMLCommand);
  gComposerCommandManager.registerCommand("cmd_insertChars", nsInsertCharsCommand);
  gComposerCommandManager.registerCommand("cmd_preferences", nsPreferencesCommand);

  gComposerCommandManager.registerCommand("cmd_listProperties",  nsListPropertiesCommand);
  gComposerCommandManager.registerCommand("cmd_pageProperties",  nsPagePropertiesCommand);
  gComposerCommandManager.registerCommand("cmd_colorProperties", nsColorPropertiesCommand);
  gComposerCommandManager.registerCommand("cmd_advancedProperties", nsAdvancedPropertiesCommand);

  gComposerCommandManager.registerCommand("cmd_image",         nsImageCommand);
  gComposerCommandManager.registerCommand("cmd_hline",         nsHLineCommand);
  gComposerCommandManager.registerCommand("cmd_table",         nsTableCommand);   // insert or edit
  gComposerCommandManager.registerCommand("cmd_editTable",     nsEditTableCommand);   // edit
  gComposerCommandManager.registerCommand("cmd_link",          nsLinkCommand);
  gComposerCommandManager.registerCommand("cmd_anchor",        nsAnchorCommand);
  gComposerCommandManager.registerCommand("cmd_insertHTML",    nsInsertHTMLCommand);
  gComposerCommandManager.registerCommand("cmd_insertBreak",   nsInsertBreakCommand);
  gComposerCommandManager.registerCommand("cmd_insertBreakAll",nsInsertBreakAllCommand);

  gComposerCommandManager.registerCommand("cmd_SelectTable",        nsSelectTableCommand);
  gComposerCommandManager.registerCommand("cmd_SelectRow",          nsSelectTableRowCommand);
  gComposerCommandManager.registerCommand("cmd_SelectColumn",       nsSelectTableColumnCommand);
  gComposerCommandManager.registerCommand("cmd_SelectCell",         nsSelectTableCellCommand);
  gComposerCommandManager.registerCommand("cmd_SelectAllCells",     nsSelectAllTableCellsCommand);
  gComposerCommandManager.registerCommand("cmd_InsertTable",        nsInsertTableCommand);
  gComposerCommandManager.registerCommand("cmd_InsertRowAbove",     nsInsertTableRowAboveCommand);
  gComposerCommandManager.registerCommand("cmd_InsertRowBelow",     nsInsertTableRowBelowCommand);
  gComposerCommandManager.registerCommand("cmd_InsertColumnBefore", nsInsertTableColumnBeforeCommand);
  gComposerCommandManager.registerCommand("cmd_InsertColumnAfter",  nsInsertTableColumnAfterCommand);
  gComposerCommandManager.registerCommand("cmd_InsertCellBefore",   nsInsertTableCellBeforeCommand);
  gComposerCommandManager.registerCommand("cmd_InsertCellAfter",    nsInsertTableCellAfterCommand);
  gComposerCommandManager.registerCommand("cmd_DeleteTable",        nsDeleteTableCommand);
  gComposerCommandManager.registerCommand("cmd_DeleteRow",          nsDeleteTableRowCommand);
  gComposerCommandManager.registerCommand("cmd_DeleteColumn",       nsDeleteTableColumnCommand);
  gComposerCommandManager.registerCommand("cmd_DeleteCell",         nsDeleteTableCellCommand);
  gComposerCommandManager.registerCommand("cmd_DeleteCellContents", nsDeleteTableCellContentsCommand);
  gComposerCommandManager.registerCommand("cmd_NormalizeTable",     nsNormalizeTableCommand);
}

//-----------------------------------------------------------------------------------
function GetComposerController()
{
  var numControllers = window.content.controllers.getControllerCount();
  
  // count down to find a controller that supplies a nsIControllerCommandManager interface
  for (var i = numControllers; i >= 0 ; i --)
  {
    var commandManager = null;
    
    try { 
      var controller = window.content.controllers.getControllerAt(i);
      var interfaceRequestor = controller.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
      if (!interfaceRequestor) continue;
    
      commandManager = interfaceRequestor.getInterface(Components.interfaces.nsIControllerCommandManager);
    } catch(ex) {
      dump("failed to get command manager number "+i+"\n");
    }

    if (commandManager)
      return commandManager;
  }

  dump("Failed to find a controller to register commands with\n");
  return null;
}

//-----------------------------------------------------------------------------------
function goUpdateComposerMenuItems(commandset)
{
  // dump("Updating commands for " + commandset.id + "\n");
  
  for (var i = 0; i < commandset.childNodes.length; i++)
  {
    var commandID = commandset.childNodes[i].getAttribute("id");
    if (commandID)
    {
      goUpdateCommand(commandID);
    }
  }
}

//-----------------------------------------------------------------------------------
function PrintObject(obj)
{
  dump("-----" + obj + "------\n");
  var names = "";
  for (var i in obj)
  {
    if (i == "value")
      names += i + ": " + obj.value + "\n";
    else if (i == "id")
      names += i + ": " + obj.id + "\n";
    else
      names += i + "\n";
  }
  
  dump(names + "-----------\n");
}

//-----------------------------------------------------------------------------------
function PrintNodeID(id)
{
  PrintObject(document.getElementById(id));
}

//-----------------------------------------------------------------------------------
var nsOpenCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
    var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
    fp.init(window, window.editorShell.GetString("OpenHTMLFile"), nsIFilePicker.modeOpen);
  
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
      if (!found)
      {
        // if the existing window is untouched, just load there
        if (PageIsEmptyAndUntouched())
        {
          window.editorShell.LoadUrl(fp.fileURL.spec);
        }
        else
        {
  	      /* open new window */
  	      window.openDialog("chrome://editor/content",
  	                        "_blank",
  	                        "chrome,dialog=no,all",
  	                        fp.fileURL.spec);
        }
      }
    }
  }
};

//-----------------------------------------------------------------------------------
var nsSaveAsCharsetCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.ok = false;
    if(window.openDialog("chrome://editor/content/EditorSaveAsCharset.xul","_blank", "chrome,close,titlebar,modal"))
    {
       if( window.ok ) 
         return editorShell.saveDocument(true, false);
    }
    return false;
  }
};
//-----------------------------------------------------------------------------------
var nsRevertCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && 
            window.editorShell.documentModified &&
            editorShell.editorDocument.location != "about:blank");
  },

  doCommand: function(aCommand)
  {
    if (window.editorShell && 
        window.editorShell.documentModified &&
        editorShell.editorDocument.location != "about:blank")
    {
      // Confirm with the user to abandon current changes
      if (commonDialogsService)
      {
        var result = {value:0};

        // Put the page title in the message string
        var title = editorShell.editorDocument.title;
        if (!title || title.length == 0)
          title = editorShell.GetTitle("untitled");

        var msg = editorShell.GetString("AbandonChanges").replace(/%title%/,title);

        commonDialogsService.UniversalDialog(
          window,
          null,
          editorShell.GetString("RevertCaption"),
          msg,
          null,
          editorShell.GetString("Revert"),
          editorShell.GetString("Cancel"),
          null,
          null,
          null,
          null,
          {value:0},
          {value:0},
          "chrome://global/skin/question-icon.gif",
          {value:"false"},
          2,
          0,
          0,
          result
          );

        // Reload page if first button (Rever) was pressed
        if(result.value == 0)
          editorShell.LoadUrl(editorShell.editorDocument.location);
      }
    }
  }
};

//-----------------------------------------------------------------------------------
var nsNewEditorCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
    NewEditorWindow();
  }
};

//-----------------------------------------------------------------------------------
var nsOpenRemoteCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
	  /* The last parameter is the current browser window.
	     Use 0 and the default checkbox will be to load into an editor
	     and loading into existing browser option is removed
	   */
	  window.openDialog( "chrome://navigator/content/openLocation.xul", "_blank", "chrome,modal", 0);
  }
};

//-----------------------------------------------------------------------------------
var nsPreviewCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    // maybe disable if we haven't saved?
    // return (window.editorShell && !window.editorShell.documentModified);   
    return (window.editorShell != null);
  },

  doCommand: function(aCommand)
  {
	  if (!editorShell.CheckAndSaveDocument(editorShell.GetString("BeforePreview")))
	    return;

	  var fileurl = "";
	  try {
	    fileurl = window.content.location;
	  } catch (e) {
	    return;
	  }

	  // CheckAndSave doesn't tell us if the user said "Don't Save",
	  // so make sure we have a url:
	  if (fileurl != "" && fileurl != "about:blank")
	  {
	    window.openDialog(getBrowserURL(), "EditorPreview", "chrome,all,dialog=no", fileurl);
	  }
  }
};

//-----------------------------------------------------------------------------------
var nsQuitCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
    goQuitApplication();
  }
};

//-----------------------------------------------------------------------------------
var nsFindCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell != null);
  },

  doCommand: function(aCommand)
  {
    window.editorShell.Find();
  }
};

//-----------------------------------------------------------------------------------
var nsFindNextCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    // we can only do this if the search pattern is non-empty. Not sure how
    // to get that from here
    return (window.editorShell != null);
  },

  doCommand: function(aCommand)
  {
    window.editorShell.FindNext();
  }
};

//-----------------------------------------------------------------------------------
var nsSpellingCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell != null) && IsSpellCheckerInstalled();
  },

  doCommand: function(aCommand)
  {
    var spellChecker = window.editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
    if (spellChecker)
    {
      // dump("Check Spelling starting...\n");
      // Start the spell checker module. Return is first misspelled word
      try {
        spellChecker.InitSpellChecker();

        // XXX: We need to read in a pref here so we can set the
        //      default language for the spellchecker!
        // spellChecker.SetCurrentDictionary();

        firstMisspelledWord = spellChecker.GetNextMisspelledWord();
      }
      catch(ex) {
        dump("*** Exception error: InitSpellChecker\n");
        return;
      }
      if( firstMisspelledWord == "")
      {
        try {
          spellChecker.UninitSpellChecker();
        }
        catch(ex) {
          dump("*** Exception error: UnInitSpellChecker\n");
          return;
        }
        // No misspelled word - tell user
        window.editorShell.AlertWithTitle(window.editorShell.GetString("CheckSpelling"),
                                          window.editorShell.GetString("NoMisspelledWord")); 
      } else {
        // Set spellChecker variable on window
        window.spellChecker = spellChecker;
        try {
          window.openDialog("chrome://editor/content/EdSpellCheck.xul", "_blank",
                  "chrome,close,titlebar,modal", "", firstMisspelledWord);
        }
        catch(ex) {
          dump("*** Exception error: SpellChecker Dialog Closing\n");
          return;
        }
      }
    }
    contentWindow.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsImageCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdImageProps.xul","_blank", "chrome,close,titlebar,modal");
  }
};

//-----------------------------------------------------------------------------------
var nsHLineCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    // Inserting an HLine is different in that we don't use properties dialog
    //  unless we are editing an existing line's attributes
    //  We get the last-used attributes from the prefs and insert immediately
  
    tagName = "hr";
    hLine = window.editorShell.GetSelectedElement(tagName);
  
    if (hLine) {
      // We only open the dialog for an existing HRule
      window.openDialog("chrome://editor/content/EdHLineProps.xul", "_blank", "chrome,close,titlebar,modal");
    } else {
      hLine = window.editorShell.CreateElementWithDefaults(tagName);
  
      if (hLine) {
        // We change the default attributes to those saved in the user prefs

        if (gPrefs) {
          var percent;
          var height;
          var shading;
          var ud = "undefined";
  
          try {
            var align = gPrefs.GetIntPref("editor.hrule.align");
            if (align == 0 ) {
              hLine.setAttribute("align", "left");
            } else if (align == 2) {
              hLine.setAttribute("align", "right");
            } else  {
              // Default is center
              hLine.setAttribute("align", "center");
            }
  	  
            var width = gPrefs.GetIntPref("editor.hrule.width");
            var percent = gPrefs.GetBoolPref("editor.hrule.width_percent");
            if (percent)
              width = width +"%";
  
            hLine.setAttribute("width", width);
  
            var height = gPrefs.GetIntPref("editor.hrule.height");
            hLine.setAttribute("size", String(height));
  
            var shading = gPrefs.GetBoolPref("editor.hrule.shading");
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
          window.editorShell.InsertElementAtSelection(hLine, true);
        } catch (e) {
          dump("Exception occured in InsertElementAtSelection\n");
        }
      }
    }
  }
};

//-----------------------------------------------------------------------------------
var nsTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    EditorInsertOrEditTable(true);
  }
};


//-----------------------------------------------------------------------------------
var nsEditTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    EditorInsertOrEditTable(false);
  }
};

//-----------------------------------------------------------------------------------
var nsLinkCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdLinkProps.xul","_blank", "chrome,close,titlebar,modal");
  }
};

//-----------------------------------------------------------------------------------
var nsAnchorCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdNamedAnchorProps.xul", "_blank", "chrome,close,titlebar,modal", "");
  }
};

//-----------------------------------------------------------------------------------
var nsInsertHTMLCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdInsSrc.xul","_blank", "chrome,close,titlebar,modal,resizable", "");
  }
};

//-----------------------------------------------------------------------------------
var nsInsertCharsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdInsertChars.xul", "_blank", "chrome,close,titlebar", "");
  }
};

//-----------------------------------------------------------------------------------
var nsInsertBreakCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return false;
  },
  doCommand: function(aCommand)
  {
    _EditorNotImplemented();
  }
};

//-----------------------------------------------------------------------------------
var nsInsertBreakAllCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return false;
  },
  doCommand: function(aCommand)
  {
    _EditorNotImplemented();
  }
};

//-----------------------------------------------------------------------------------
var nsListPropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdListProps.xul","_blank", "chrome,close,titlebar,modal");
  }
};


//-----------------------------------------------------------------------------------
var nsPagePropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdPageProps.xul","_blank", "chrome,close,titlebar,modal,resizable", "");
  }
};

//-----------------------------------------------------------------------------------

var nsAdvancedPropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    // Launch AdvancedEdit dialog for the selected element
    var element = window.editorShell.GetSelectedElement("");
    if (element)
      window.openDialog("chrome://editor/content/EdAdvancedEdit.xul", "_blank", "chrome,close,titlebar,modal,resizable=yes", "", element);
  }
};

//-----------------------------------------------------------------------------------
var nsColorPropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdColorProps.xul","_blank", "chrome,close,titlebar,modal", "");
  }
};


//-----------------------------------------------------------------------------------
var nsEditHTMLCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    _EditorNotImplemented(); //TODO: IMPLEMENT ME!
  }
};

//-----------------------------------------------------------------------------------
// TABLE EDITING COMMANDS
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
  if (editorShell.GetFirstSelectedCell())
    menuText = GetString("JoinSelectedCells");
  else
    menuText = GetString("JoinCellToRight");

  document.getElementById("menu_tableJoinCells").setAttribute("value",menuText);

  // Set platform-specific hints for how to select cells
  if (gIsWin) osKey = "XulKeyWin";
  if (gIsMac) osKey = "XulKeyMac";
  if (gIsUNIX) osKey = "XulKeyUnix";

  var DragStr = GetString(osKey)+GetString("Drag");
  var ClickStr = GetString(osKey)+GetString("Click");
  var DelStr = GetString(gIsMac ? "Clear" : "Del");

  document.getElementById("menu_DeleteCell").setAttribute("acceltext",ClickStr);
  document.getElementById("menu_SelectRow").setAttribute("acceltext",DragStr);
  document.getElementById("menu_SelectColumn").setAttribute("acceltext",DragStr);
  document.getElementById("menu_SelectAllCells").setAttribute("acceltext",DragStr);
  // And add "Del" or "Clear"
  document.getElementById("menu_DeleteCellContents").setAttribute("acceltext",DelStr);

  // Set enable states for all table commands
  goUpdateTableMenuItems(document.getElementById("composerTableMenuItems"));
}


function goUpdateTableMenuItems(commandset)
{
  // Insert table can be done any time (if editable document)
  goUpdateCommand("cmd_InsertTable");

  var enabled = false; 
  var enabledIfTable = false;
  if (window.editorShell && window.editorShell.documentEditable)
  {
    var selectedCountObj = new Object();
    var tagNameObj = new Object();
    var element = editorShell.GetSelectedOrParentTableElement(tagNameObj, selectedCountObj);  
    if (element)
    {
      // We can delete or normalize table if inside a table 
      //  or any table element is selected
      enabledIfTable = true;  
      
      // All others require being inside a cell or selected cell
      enable = (tagNameObj.value == "td");
    }
  }

  goSetCommandEnabled("cmd_DeleteTable", enabledIfTable);
  goSetCommandEnabled("cmd_NormalizeTable", enabledIfTable);

  // Loop through command nodes to set all the others
  for (var i = 0; i < commandset.childNodes.length; i++)
  {
    var commandID = commandset.childNodes[i].getAttribute("id");
    if (commandID && 
        commandID != "cmd_InsertTable" && 
        commandID != "cmd_DeleteTable" &&
        commandID != "cmd_NormalizeTable")
    {
    	goSetCommandEnabled(commandID, enabled);
    }
  }
}

var nsSelectTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("table"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.SelectTable();
  }
};

var nsSelectTableRowCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.SelectTableRow();
  }
};

var nsSelectTableColumnCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
dump("nsSelectTableColumnCommand\n");
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.SelectTableColumn();
  }
};

var nsSelectTableCellCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.SelectTableCell();
  }
};

var nsSelectAllTableCellsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.SelectAllTableCells();
  }
};

var nsInsertTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.InsertTable();
  }
};

var nsInsertTableRowAboveCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.InsertTableRow(false);
  }
};

var nsInsertTableRowBelowCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.InsertTableRow(true);
  }
};

var nsInsertTableColumnBeforeCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.InsertTableColumn(false);
  }
};

var nsInsertTableColumnAfterCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.InsertTableColumn(true);
  }
};

var nsInsertTableCellBeforeCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.InsertTableCell(false);
  }
};

var nsInsertTableCellAfterCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.InsertTableCell(true);
  }
};

var nsDeleteTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("table"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.DeleteTable();        
  }
};

var nsDeleteTableRowCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.DeleteTableRow(1);   
  }
};

var nsDeleteTableColumnCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.DeleteTableColumn(1); 
  }
};

var nsDeleteTableCellCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.DeleteTableCell(1);   
  }
};

var nsDeleteTableCellContentsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.DeleteTableCellContents();
  }
};

var nsNormalizeTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("table"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.NormalizeTable();
  }
};

var nsJoinTableCellsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.JoinTableCells();
  }
};

var nsSplitTableCellCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable &&
            null != window.editorShell.GetElementOrParentByTagName("td"));
  },
  doCommand: function(aCommand)
  {
    if (isCommandEnabled)
      window.editorShell.SplitTableCell();
  }
};


//-----------------------------------------------------------------------------------
var nsPreferencesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;
  },
  doCommand: function(aCommand)
  {
    goPreferences('navigator.xul', 'chrome://communicator/content/pref/pref-composer.xul','editor');
  }
};


