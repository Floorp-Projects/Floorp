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
 *    Ryan Cassin (rcassin@supernova.org)
 */

/* Implementations of nsIControllerCommand for composer commands */


var gHTMLEditorCommandManager = null;
var gComposerWindowCommandManager = null;
var commonDialogsService = Components.classes["component://netscape/appshell/commonDialogs"].getService();
commonDialogsService = commonDialogsService.QueryInterface(Components.interfaces.nsICommonDialogs);

//-----------------------------------------------------------------------------------
function SetupHTMLEditorCommands()
{
  gHTMLEditorCommandManager = GetHTMLEditorController();
  if (!gHTMLEditorCommandManager)
    return;
  
  dump("Registering commands\n");
  
  gHTMLEditorCommandManager.registerCommand("cmd_find",       nsFindCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_findNext",   nsFindNextCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_spelling",   nsSpellingCommand);

  gHTMLEditorCommandManager.registerCommand("cmd_insertChars", nsInsertCharsCommand);

  gHTMLEditorCommandManager.registerCommand("cmd_listProperties",  nsListPropertiesCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_pageProperties",  nsPagePropertiesCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_colorProperties", nsColorPropertiesCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_advancedProperties", nsAdvancedPropertiesCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_objectProperties",   nsObjectPropertiesCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_removeLinks",        nsRemoveLinksCommand);
  
  gHTMLEditorCommandManager.registerCommand("cmd_image",         nsImageCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_hline",         nsHLineCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_link",          nsLinkCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_anchor",        nsAnchorCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_insertHTML",    nsInsertHTMLCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_insertBreak",   nsInsertBreakCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_insertBreakAll",nsInsertBreakAllCommand);

  gHTMLEditorCommandManager.registerCommand("cmd_table",              nsInsertOrEditTableCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_editTable",          nsEditTableCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_SelectTable",        nsSelectTableCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_SelectRow",          nsSelectTableRowCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_SelectColumn",       nsSelectTableColumnCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_SelectCell",         nsSelectTableCellCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_SelectAllCells",     nsSelectAllTableCellsCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_InsertTable",        nsInsertTableCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_InsertRowAbove",     nsInsertTableRowAboveCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_InsertRowBelow",     nsInsertTableRowBelowCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_InsertColumnBefore", nsInsertTableColumnBeforeCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_InsertColumnAfter",  nsInsertTableColumnAfterCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_InsertCellBefore",   nsInsertTableCellBeforeCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_InsertCellAfter",    nsInsertTableCellAfterCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_DeleteTable",        nsDeleteTableCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_DeleteRow",          nsDeleteTableRowCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_DeleteColumn",       nsDeleteTableColumnCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_DeleteCell",         nsDeleteTableCellCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_DeleteCellContents", nsDeleteTableCellContentsCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_JoinTableCells",     nsJoinTableCellsCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_SplitTableCell",     nsSplitTableCellCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_TableOrCellColor",   nsTableOrCellColorCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_NormalizeTable",     nsNormalizeTableCommand);
  gHTMLEditorCommandManager.registerCommand("cmd_FinishHTMLSource",   nsFinishHTMLSource);
  gHTMLEditorCommandManager.registerCommand("cmd_CancelHTMLSource",   nsCancelHTMLSource);
}

function SetupComposerWindowCommands()
{
  // Create a command controller and register commands
  //   specific to Web Composer window (file-related commands, HTML Source...)
  // IMPORTANT: For each of these commands, the doCommand method 
  //            must first call FinishHTMLSource() 
  //            to go from HTML Source mode to any other edit mode

  var gComposerWindowCommandManager = window.controllers;

  if (!gComposerWindowCommandManager) return;

  var composerController = Components.classes["component://netscape/editor/composercontroller"].createInstance();
  if (!composerController)
  {
    dump("Failed to create composerController\n");
    return;
  }

  var editorController = composerController.QueryInterface(Components.interfaces.nsIEditorController);
  if (!editorController)
  {
    dump("Failed to get interface for nsIEditorController\n");
    return;
  }

  // Note: We init with the editorShell for the main composer window, not the HTML Source textfield?
  editorController.Init(window.editorShell);

  var interfaceRequestor = composerController.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
  if (!interfaceRequestor)
  {
    dump("Failed to get iterfaceRequestor for composerController\n");
    return;
  }
    

  // Get the nsIControllerCommandManager interface we need to register more commands
  var commandManager = interfaceRequestor.getInterface(Components.interfaces.nsIControllerCommandManager);
  if (!commandManager)
  {
    dump("Failed to get interface for nsIControllerCommandManager\n");
    return;
  }

  // File-related commands
  commandManager.registerCommand("cmd_newEditor",     nsNewEditorCommand);
  commandManager.registerCommand("cmd_open",          nsOpenCommand);
  commandManager.registerCommand("cmd_save",          nsSaveCommand);
  commandManager.registerCommand("cmd_saveAs",        nsSaveAsCommand);
  commandManager.registerCommand("cmd_saveAsCharset", nsSaveAsCharsetCommand);
  commandManager.registerCommand("cmd_revert",        nsRevertCommand);
  commandManager.registerCommand("cmd_openRemote",    nsOpenRemoteCommand);
  commandManager.registerCommand("cmd_preview",       nsPreviewCommand);
  commandManager.registerCommand("cmd_editSendPage",  nsSendPageCommand);
  commandManager.registerCommand("cmd_quit",          nsQuitCommand);
  commandManager.registerCommand("cmd_close",         nsCloseCommand);
  commandManager.registerCommand("cmd_preferences",   nsPreferencesCommand);

  // Edit Mode commands
  commandManager.registerCommand("cmd_NormalMode",         nsNormalModeCommand);
  commandManager.registerCommand("cmd_AllTagsMode",        nsAllTagsModeCommand);
  commandManager.registerCommand("cmd_HTMLSourceMode",     nsHTMLSourceModeCommand);
  commandManager.registerCommand("cmd_PreviewMode",        nsPreviewModeCommand);
  commandManager.registerCommand("cmd_FinishHTMLSource",   nsFinishHTMLSource);
  commandManager.registerCommand("cmd_CancelHTMLSource",   nsCancelHTMLSource);


  gComposerWindowCommandManager.insertControllerAt(0, editorController);
}

//-----------------------------------------------------------------------------------
function GetHTMLEditorController()
{
  var numControllers = window._content.controllers.getControllerCount();
  
  // count down to find a controller that supplies a nsIControllerCommandManager interface
  for (var i = numControllers; i >= 0 ; i --)
  {
    var commandManager = null;
    
    try { 
      var controller = window._content.controllers.getControllerAt(i);
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
  
    // When loading into Composer, direct user to prefer HTML files and text files,
    //   so we call separately to control the order of the filter list
    fp.appendFilters(nsIFilePicker.filterHTML);
    fp.appendFilters(nsIFilePicker.filterText);
    fp.appendFilters(nsIFilePicker.filterAll);

    /* doesn't handle *.shtml files */
    try {
      fp.show();
      /* need to handle cancel (uncaught exception at present) */
    }
    catch (ex) {
      dump("filePicker.chooseInputFile threw an exception\n");
    }
  
    /* This checks for already open window and activates it... 
     * note that we have to test the native path length
     *  since fileURL.spec will be "file:///" if no filename picked (Cancel button used)
     */
    if (fp.file && fp.file.path.length > 0) {
      EditorOpenUrl(fp.fileURL.spec);
    }
  }
};

//-----------------------------------------------------------------------------------
var nsSaveCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return window.editorShell && 
      (window.editorShell.documentModified || window.editorShell.editorDocument.location == "about:blank");
  },
  
  doCommand: function(aCommand)
  {
    if (window.editorShell)
    {
      FinishHTMLSource(); // In editor.js
      var doSaveAs = window.editorShell.editorDocument.location == "about:blank";
      return window.editorShell.saveDocument(doSaveAs, false);
    }
  }
}

var nsSaveAsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },

  doCommand: function(aCommand)
  {
    if (window.editorShell)
    {
      FinishHTMLSource();
      return window.editorShell.saveDocument(true, false);
    }
  }
}

var nsSaveAsCharsetCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {    
    FinishHTMLSource();
    window.ok = false;
    if (window.openDialog("chrome://editor/content/EditorSaveAsCharset.xul","_blank", "chrome,close,titlebar,modal"))
    {
      window.ok = window.editorShell.saveDocument(true, false);
    }
    window._content.focus();
    return window.ok;
  }
};

//-----------------------------------------------------------------------------------
var nsRevertCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && 
            window.editorShell.documentModified &&
            window.editorShell.editorDocument.location != "about:blank");
  },

  doCommand: function(aCommand)
  {
    // Confirm with the user to abandon current changes
    if (commonDialogsService)
    {
      var result = {value:0};

      // Put the page title in the message string
      var title = window.editorShell.editorDocument.title;
      if (!title || title.length == 0)
        title = window.editorShell.GetTitle("untitled");

      var msg = window.editorShell.GetString("AbandonChanges").replace(/%title%/,title);

      commonDialogsService.UniversalDialog(
        window,
        null,
        window.editorShell.GetString("RevertCaption"),
        msg,
        null,
        window.editorShell.GetString("Revert"),
        window.editorShell.GetString("Cancel"),
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

      // Reload page if first button (Revert) was pressed
      if(result.value == 0)
      {
        FinishHTMLSource();
        window.editorShell.LoadUrl(editorShell.editorDocument.location);
      }
    }
  }
};

//-----------------------------------------------------------------------------------
var nsCloseCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return window.editorShell;
  },
  
  doCommand: function(aCommand)
  {
    CloseWindow();
  }
};

function CloseWindow()
{
  FinishHTMLSource();

  // Check to make sure document is saved. "true" means allow "Don't Save" button,
  //   so user can choose to close without saving
  if (CheckAndSaveDocument(window.editorShell.GetString("BeforeClosing"), true)) 
  {
    if (window.InsertCharWindow)
      SwitchInsertCharToAnotherEditorOrClose();

    window.editorShell.CloseWindowWithoutSaving();
  }
}

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
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsPreviewCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell != null && (DocumentHasBeenSaved() || window.editorShell.documentModified));
  },

  doCommand: function(aCommand)
  {
    FinishHTMLSource();

	  // Don't continue if user canceled during prompt for saving
    // DocumentHasBeenSaved will test if we have a URL and suppress "Don't Save" button if not
    if (!CheckAndSaveDocument(window.editorShell.GetString("BeforePreview"), DocumentHasBeenSaved()))
	    return;

    // Check if we saved again just in case?
	  if (DocumentHasBeenSaved())
	    window.openDialog(getBrowserURL(), "EditorPreview", "chrome,all,dialog=no", window._content.location);
  }
};

//-----------------------------------------------------------------------------------
var nsSendPageCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell != null && (DocumentHasBeenSaved() || window.editorShell.documentModified));
  },

  doCommand: function(aCommand)
  {
    FinishHTMLSource();

	  // Don't continue if user canceled during prompt for saving
    // DocumentHasBeenSaved will test if we have a URL and suppress "Don't Save" button if not
    if (!CheckAndSaveDocument(window.editorShell.GetString("SendPageReason"), DocumentHasBeenSaved()))
	    return;

    // Check if we saved again just in case?
	  if (DocumentHasBeenSaved())
    {
      // Lauch Messenger Composer window with current page as contents
      var pageTitle = window.editorShell.editorDocument.title;
      var pageUrl = window.editorShell.editorDocument.location;
      window.openDialog("chrome://messenger/content/messengercompose/messengercompose.xul", "_blank", 
                          "chrome,all,dialog=no", "attachment='" + pageUrl + "',body='" + pageUrl +
                          "',subject='" + pageTitle + "',bodyislink=true");
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
    // In editor.js
    FinishHTMLSource();
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
      var firstMisspelledWord;
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
          window._content.focus();
          return;
        }
      }
    }
    window._content.focus();
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
    window._content.focus();
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

    var tagName = "hr";
    var hLine = window.editorShell.GetSelectedElement(tagName);

    if (hLine) {
      // We only open the dialog for an existing HRule
      window.openDialog("chrome://editor/content/EdHLineProps.xul", "_blank", "chrome,close,titlebar,modal");
      window._content.focus();
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
var nsLinkCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdLinkProps.xul","_blank", "chrome,close,titlebar,modal");
    window._content.focus();
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
    window._content.focus();
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
    window._content.focus();
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
    EditorFindOrCreateInsertCharWindow();
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
    window._content.focus();
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
    window.openDialog("chrome://editor/content/EdPageProps.xul","_blank", "chrome,close,titlebar,modal", "");
  }
};

//-----------------------------------------------------------------------------------
var nsObjectPropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    var isEnabled = false;
    if (window.editorShell && window.editorShell.documentEditable)
    {
      try {
      // Launch Object properties for appropriate selected element 
        isEnabled = (GetSelectedElementOrParentCellOrLink() != null ||
                window.editorShell.GetSelectedElement("href") != null);
      } catch(e)
      {
      }
    }
    return isEnabled;
  },
  doCommand: function(aCommand)
  {
    // Launch Object properties for appropriate selected element 
    var element = GetSelectedElementOrParentCellOrLink();
    if (element)
    {
      var name = element.nodeName.toLowerCase();
      switch (name)
      {
        case 'img':
          goDoCommand("cmd_image");
          break;
        case 'hr':
          goDoCommand("cmd_hline");
          break;
        case 'table':
          EditorInsertOrEditTable(false);
          break;
        case 'td':
          EditorTableCellProperties();
          break;
        case 'a':
          if (element.name)
          {
            goDoCommand("cmd_anchor");
          }
          else if(element.href)
          {
            goDoCommand("cmd_link");
          }
          break;
        default:
          doAdvancedProperties(element);
          break;
      }
    } else {
      // We get a partially-selected link if asked for specifically
      element = window.editorShell.GetSelectedElement("href");
      if (element)
        goDoCommand("cmd_link");
    }
    window._content.focus();
  }
};

function doAdvancedProperties(element)
{
  if (element)
  {
    window.openDialog("chrome://editor/content/EdAdvancedEdit.xul", "_blank", "chrome,close,titlebar,modal,resizable=yes", "", element);
    window._content.focus();
  }
}

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
    doAdvancedProperties(element);
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
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsRemoveLinksCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    // We could see if there's any link in selection, but it doesn't seem worth the work!
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.editorShell.RemoveTextProperty("href", "");
    window._content.focus();
  }
};


//-----------------------------------------------------------------------------------
var nsNormalModeCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true; //(window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    if (gEditorDisplayMode != DisplayModeNormal)
      SetEditMode(DisplayModeNormal);
  }
};

var nsAllTagsModeCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    if (gEditorDisplayMode != DisplayModeAllTags)
      SetEditMode(DisplayModeAllTags);
  }
};

var nsHTMLSourceModeCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    if (gEditorDisplayMode != DisplayModeSource)
      SetEditMode(DisplayModeSource);
  }
};

var nsPreviewModeCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    FinishHTMLSource();
    if (gEditorDisplayMode != DisplayModePreview)
      SetEditMode(DisplayModePreview);
  }
};

//-----------------------------------------------------------------------------------
var nsInsertOrEditTableCommand =
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
    return IsInTable();
  },
  doCommand: function(aCommand)
  {
    EditorInsertOrEditTable(false);
  }
};

//-----------------------------------------------------------------------------------
var nsSelectTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTable();
  },
  doCommand: function(aCommand)
  {
//dump("nsSelectTableCommand\n");
    window.editorShell.SelectTable();
    window._content.focus();
  }
};

var nsSelectTableRowCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
//dump("nsSelectTableRowCommand\n");
    window.editorShell.SelectTableRow();
    window._content.focus();
  }
};

var nsSelectTableColumnCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
//dump("nsSelectTableColumnCommand\n");
    window.editorShell.SelectTableColumn();
    window._content.focus();
  }
};

var nsSelectTableCellCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
//dump("nsSelectTableCellCommand\n");
    window.editorShell.SelectTableCell();
    window._content.focus();
  }
};

var nsSelectAllTableCellsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTable();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.SelectAllTableCells();
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsInsertTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    EditorInsertTable();
  }
};

var nsInsertTableRowAboveCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.InsertTableRow(1, false);
    window._content.focus();
  }
};

var nsInsertTableRowBelowCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.InsertTableRow(1,true);
    window._content.focus();
  }
};

var nsInsertTableColumnBeforeCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.InsertTableColumn(1, false);
    window._content.focus();
  }
};

var nsInsertTableColumnAfterCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.InsertTableColumn(1, true);
    window._content.focus();
  }
};

var nsInsertTableCellBeforeCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.InsertTableCell(1, false);
  }
};

var nsInsertTableCellAfterCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.InsertTableCell(1, true);
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsDeleteTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTable();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.DeleteTable();        
    window._content.focus();
  }
};

var nsDeleteTableRowCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
//dump("nsDeleteTableRowCommand: doCommand\n");
    window.editorShell.DeleteTableRow(1);
    window._content.focus();
  }
};

var nsDeleteTableColumnCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.DeleteTableColumn(1); 
    window._content.focus();
  }
};

var nsDeleteTableCellCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.DeleteTableCell(1);   
    window._content.focus();
  }
};

var nsDeleteTableCellContentsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTableCell();
  },
  doCommand: function(aCommand)
  {
    window.editorShell.DeleteTableCellContents();
    window._content.focus();
  }
};


//-----------------------------------------------------------------------------------
var nsNormalizeTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTable();
  },
  doCommand: function(aCommand)
  {
    // Use nsnull to let editor find table enclosing current selection
    window.editorShell.NormalizeTable(null);
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsJoinTableCellsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    if (window.editorShell && window.editorShell.documentEditable)
    {
      var tagNameObj = new Object;
      var countObj = new Object;
      var cell = window.editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);

      // We need a cell and either > 1 selected cell or a cell to the right
      //  (this cell may originate in a row spanned from above current row)
      // Note that editorShell returns "td" for "th" also.
      // (tis is a pain! Editor and gecko use lowercase tagNames, JS uses uppercase!)
      if( cell && (tagNameObj.value == "td"))
      {
        // Selected cells
        if (countObj.value > 1) return true;

        var colSpan = cell.getAttribute("colspan");
        if (!colSpan) colSpan = 1;

        // cells with 0 span should never have cells to the right
        // (if there is, user can select the 2 cells to join them)
        return (colSpan != 0 &&
                window.editorShell.GetCellAt(null, 
                                   window.editorShell.GetRowIndex(cell), 
                                   window.editorShell.GetColumnIndex(cell) + colSpan));
      }
    }
    return false;
  },
  doCommand: function(aCommand)
  {
    // Param: Don't merge non-contiguous cells
    window.editorShell.JoinTableCells(false);
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsSplitTableCellCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    if (window.editorShell && window.editorShell.documentEditable)
    {
      var tagNameObj = new Object;
      var countObj = new Object;
      var cell = window.editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);
      // We need a cell parent and there's just 1 selected cell 
      // or selection is entirely inside 1 cell
      if ( cell && (tagNameObj.value == "td") && 
           countObj.value <= 1 &&
           IsSelectionInOneCell() )
      {
        var colSpan = cell.getAttribute("colspan");
        var rowSpan = cell.getAttribute("colspan");
        if (!colSpan) colSpan = 1;
        if (!rowSpan) rowSpan = 1;
        return (colSpan > 1  || rowSpan > 1 ||
                colSpan == 0 || rowSpan == 0);
      }
    }
    return false;
  },
  doCommand: function(aCommand)
  {
    window.editorShell.SplitTableCell();
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsTableOrCellColorCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return IsInTable();
  },
  doCommand: function(aCommand)
  {
    EditorSelectColor("TableOrCell");
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
    goPreferences('navigator.xul', 'chrome://editor/content/pref-composer.xul','editor');
  }
};


var nsFinishHTMLSource =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;
  },
  doCommand: function(aCommand)
  {
    // In editor.js
    FinishHTMLSource();
  }
};

var nsCancelHTMLSource =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;
  },
  doCommand: function(aCommand)
  {
    // In editor.js
    CancelHTMLSource();
  }
};
