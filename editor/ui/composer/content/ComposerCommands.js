/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Simon Fraser (sfraser@netscape.com)
 *    Ryan Cassin (rcassin@supernova.org)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Implementations of nsIControllerCommand for composer commands */


//-----------------------------------------------------------------------------------
function SetupHTMLEditorCommands()
{
  var controller = GetEditorController();
  if (!controller)
    return;
  

  // Include everthing a text editor does
  SetupTextEditorCommands();

  //dump("Registering HTML editor commands\n");

  controller.registerCommand("cmd_renderedHTMLEnabler",           nsDummyHTMLCommand);

  controller.registerCommand("cmd_listProperties",  nsListPropertiesCommand);
  controller.registerCommand("cmd_pageProperties",  nsPagePropertiesCommand);
  controller.registerCommand("cmd_colorProperties", nsColorPropertiesCommand);
  controller.registerCommand("cmd_advancedProperties", nsAdvancedPropertiesCommand);
  controller.registerCommand("cmd_objectProperties",   nsObjectPropertiesCommand);
  controller.registerCommand("cmd_removeLinks",        nsRemoveLinksCommand);
  
  controller.registerCommand("cmd_image",         nsImageCommand);
  controller.registerCommand("cmd_hline",         nsHLineCommand);
  controller.registerCommand("cmd_link",          nsLinkCommand);
  controller.registerCommand("cmd_anchor",        nsAnchorCommand);
  controller.registerCommand("cmd_insertHTML",    nsInsertHTMLCommand);
  controller.registerCommand("cmd_insertBreak",   nsInsertBreakCommand);
  controller.registerCommand("cmd_insertBreakAll",nsInsertBreakAllCommand);

  controller.registerCommand("cmd_table",              nsInsertOrEditTableCommand);
  controller.registerCommand("cmd_editTable",          nsEditTableCommand);
  controller.registerCommand("cmd_SelectTable",        nsSelectTableCommand);
  controller.registerCommand("cmd_SelectRow",          nsSelectTableRowCommand);
  controller.registerCommand("cmd_SelectColumn",       nsSelectTableColumnCommand);
  controller.registerCommand("cmd_SelectCell",         nsSelectTableCellCommand);
  controller.registerCommand("cmd_SelectAllCells",     nsSelectAllTableCellsCommand);
  controller.registerCommand("cmd_InsertTable",        nsInsertTableCommand);
  controller.registerCommand("cmd_InsertRowAbove",     nsInsertTableRowAboveCommand);
  controller.registerCommand("cmd_InsertRowBelow",     nsInsertTableRowBelowCommand);
  controller.registerCommand("cmd_InsertColumnBefore", nsInsertTableColumnBeforeCommand);
  controller.registerCommand("cmd_InsertColumnAfter",  nsInsertTableColumnAfterCommand);
  controller.registerCommand("cmd_InsertCellBefore",   nsInsertTableCellBeforeCommand);
  controller.registerCommand("cmd_InsertCellAfter",    nsInsertTableCellAfterCommand);
  controller.registerCommand("cmd_DeleteTable",        nsDeleteTableCommand);
  controller.registerCommand("cmd_DeleteRow",          nsDeleteTableRowCommand);
  controller.registerCommand("cmd_DeleteColumn",       nsDeleteTableColumnCommand);
  controller.registerCommand("cmd_DeleteCell",         nsDeleteTableCellCommand);
  controller.registerCommand("cmd_DeleteCellContents", nsDeleteTableCellContentsCommand);
  controller.registerCommand("cmd_JoinTableCells",     nsJoinTableCellsCommand);
  controller.registerCommand("cmd_SplitTableCell",     nsSplitTableCellCommand);
  controller.registerCommand("cmd_TableOrCellColor",   nsTableOrCellColorCommand);
  controller.registerCommand("cmd_NormalizeTable",     nsNormalizeTableCommand);
  controller.registerCommand("cmd_FinishHTMLSource",   nsFinishHTMLSource);
  controller.registerCommand("cmd_CancelHTMLSource",   nsCancelHTMLSource);
  controller.registerCommand("cmd_smiley",             nsSetSmiley);
  controller.registerCommand("cmd_buildRecentPagesMenu", nsBuildRecentPagesMenu);
  controller.registerCommand("cmd_ConvertToTable",     nsConvertToTable);
}

function SetupTextEditorCommands()
{
  var controller = GetEditorController();
  if (!controller)
    return;
  
  //dump("Registering plain text editor commands\n");
  
  controller.registerCommand("cmd_find",       nsFindCommand);
  controller.registerCommand("cmd_findNext",   nsFindNextCommand);
  controller.registerCommand("cmd_spelling",   nsSpellingCommand);
  controller.registerCommand("cmd_validate",   nsValidateCommand);
  controller.registerCommand("cmd_checkLinks", nsCheckLinksCommand);
  controller.registerCommand("cmd_insertChars", nsInsertCharsCommand);
}

function SetupComposerWindowCommands()
{
  // Create a command controller and register commands
  //   specific to Web Composer window (file-related commands, HTML Source...)
  // IMPORTANT: For each of these commands, the doCommand method 
  //            must first call FinishHTMLSource() 
  //            to go from HTML Source mode to any other edit mode

  var windowCommandManager = window.controllers;

  if (!windowCommandManager) return;

  var composerController = Components.classes["@mozilla.org/editor/composercontroller;1"].createInstance();
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
  commandManager.registerCommand("cmd_open",           nsOpenCommand);
  commandManager.registerCommand("cmd_save",           nsSaveCommand);
  commandManager.registerCommand("cmd_saveAs",         nsSaveAsCommand);
  commandManager.registerCommand("cmd_exportToText",   nsExportToTextCommand);
  commandManager.registerCommand("cmd_saveAsCharset",  nsSaveAsCharsetCommand);
  commandManager.registerCommand("cmd_revert",         nsRevertCommand);
  commandManager.registerCommand("cmd_openRemote",     nsOpenRemoteCommand);
  commandManager.registerCommand("cmd_preview",        nsPreviewCommand);
  commandManager.registerCommand("cmd_editSendPage",   nsSendPageCommand);
  commandManager.registerCommand("cmd_print",          nsPrintCommand);
  commandManager.registerCommand("cmd_printSetup",     nsPrintSetupCommand);
  commandManager.registerCommand("cmd_quit",           nsQuitCommand);
  commandManager.registerCommand("cmd_close",          nsCloseCommand);
  commandManager.registerCommand("cmd_preferences",    nsPreferencesCommand);

  // Edit Mode commands
  if (window.editorShell.editorType == "html")
  {
    commandManager.registerCommand("cmd_NormalMode",         nsNormalModeCommand);
    commandManager.registerCommand("cmd_AllTagsMode",        nsAllTagsModeCommand);
    commandManager.registerCommand("cmd_HTMLSourceMode",     nsHTMLSourceModeCommand);
    commandManager.registerCommand("cmd_PreviewMode",        nsPreviewModeCommand);
    commandManager.registerCommand("cmd_FinishHTMLSource",   nsFinishHTMLSource);
    commandManager.registerCommand("cmd_CancelHTMLSource",   nsCancelHTMLSource);
  }

  windowCommandManager.insertControllerAt(0, editorController);
}

//-----------------------------------------------------------------------------------
function GetEditorController()
{
  var numControllers = window._content.controllers.getControllerCount();
    
  // count down to find a controller that supplies a nsIControllerCommandManager interface
  for (var i = numControllers-1; i >= 0 ; i --)
  {
    var commandManager = null;
    
    try { 
      var controller = window._content.controllers.getControllerAt(i);
      
      var interfaceRequestor = controller.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
      if (!interfaceRequestor) continue;
    
      commandManager = interfaceRequestor.getInterface(Components.interfaces.nsIControllerCommandManager);
    }
    catch(ex)
    {
        //dump(ex + "\n");
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
var nsDummyHTMLCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },

  doCommand: function(aCommand)
  {
    // do nothing
    dump("Hey, who's calling the dummy command?\n");
  }

};

//-----------------------------------------------------------------------------------
var nsOpenCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
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
      (window.editorShell.documentModified || 
       window.editorShell.editorDocument.location == "about:blank" ||
       window.gHTMLSourceChanged);
  },
  
  doCommand: function(aCommand)
  {
    if (window.editorShell)
    {
      FinishHTMLSource(); // In editor.js
      var doSaveAs = window.editorShell.editorDocument.location == "about:blank";
      var result = window.editorShell.saveDocument(doSaveAs, false, editorShell.contentsMIMEType);
      window._content.focus();
      return result;
    }
    return false;
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
      var result = window.editorShell.saveDocument(true, false, editorShell.contentsMIMEType);
      window._content.focus();
      return result;
    }
    return false;
  }
}

var nsExportToTextCommand =
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
      var result = window.editorShell.saveDocument(true, true, "text/plain");
      window._content.focus();
      return result;
    }
    return false;
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
    window.exportToText = false;
    window.openDialog("chrome://editor/content/EditorSaveAsCharset.xul","_blank", "chrome,close,titlebar,modal,resizable=yes");

    if (window.newTitle != null) {
      try {
        editorShell.SetDocumentTitle(window.newTitle);
      } 
      catch (ex) {}
    }    

    if (window.ok)
    {
      if (window.exportToText)
      {
        window.ok = window.editorShell.saveDocument(true, true, "text/plain");
      }
      else
      {
        window.ok = window.editorShell.saveDocument(true, false, editorShell.contentsMIMEType);
      }
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
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);

    if (promptService)
    {
      var result = {value:0};

      // Put the page title in the message string
      var title = window.editorShell.GetDocumentTitle();
      if (!title)
        title = window.editorShell.GetString("untitled");

      var msg = window.editorShell.GetString("AbandonChanges").replace(/%title%/,title);

      promptService.confirmEx(window, window.editorShell.GetString("RevertCaption"), msg,
  						      (promptService.BUTTON_TITLE_REVERT * promptService.BUTTON_POS_0) +
  						      (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1),
  						      null, null, null, null, {value:0}, result);

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
	  window.openDialog( "chrome://communicator/content/openLocation.xul", "_blank", "chrome,modal,titlebar", 0);
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsPreviewCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && IsEditorContentHTML() && (DocumentHasBeenSaved() || window.editorShell.documentModified));
  },

  doCommand: function(aCommand)
  {
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
	  // Don't continue if user canceled during prompt for saving
    // DocumentHasBeenSaved will test if we have a URL and suppress "Don't Save" button if not
    if (!CheckAndSaveDocument(window.editorShell.GetString("SendPageReason"), DocumentHasBeenSaved()))
	    return;

    // Check if we saved again just in case?
	  if (DocumentHasBeenSaved())
    {
      // Launch Messenger Composer window with current page as contents
      var pageTitle = window.editorShell.editorDocument.title;
      var pageUrl = window.editorShell.editorDocument.location.href;
      window.openDialog( "chrome://messenger/content/messengercompose/messengercompose.xul", "_blank", 
                         "chrome,all,dialog=no", 
                         "attachment='" + pageUrl.replace(/\,/g, "%2C") + "',body='" + pageUrl +
                         "',subject='" + pageTitle + "',bodyislink=true");
    }
  }
};

//-----------------------------------------------------------------------------------
var nsPrintCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
    // In editor.js
    FinishHTMLSource();
    window.editorShell.Print();
  }
};

//-----------------------------------------------------------------------------------
var nsPrintSetupCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
    // In editor.js
    FinishHTMLSource();
    goPageSetup();
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
    return (window.editorShell && !IsInHTMLSourceMode());
  },

  doCommand: function(aCommand)
  {
    window.editorShell.Replace();
  }
};

//-----------------------------------------------------------------------------------
var nsFindNextCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    // we can only do this if the search pattern is non-empty. Not sure how
    // to get that from here
    return (window.editorShell && !IsInHTMLSourceMode());
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
    return (window.editorShell != null) && !IsInHTMLSourceMode() && IsSpellCheckerInstalled();
  },

  doCommand: function(aCommand)
  {
    try {
      window.openDialog("chrome://editor/content/EdSpellCheck.xul", "_blank",
              "chrome,close,titlebar,modal", "");
    }
    catch(ex) {
      dump("*** Exception error: SpellChecker Dialog Closing\n");
    }
    window._content.focus();
  }
};

// Validate using http://validator.w3.org/file-upload.html
var URL2Validate;
var nsValidateCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell != null);
  },

  doCommand: function(aCommand)
  {
    // If the document hasn't been modified,
    // then just validate the current url.
    if (editorShell.documentModified || gHTMLSourceChanged)
    {
      if (!CheckAndSaveDocument(window.editorShell.GetString("BeforeValidate"),
                                false))
        return;

      // Check if we saved again just in case?
      if (!DocumentHasBeenSaved())    // user hit cancel?
        return;
    }

    URL2Validate = window.editorShell.editorDocument.location;
    // See if it's a file:
    var ifile = Components.classes["@mozilla.org/file/local;1"].createInstance().QueryInterface(Components.interfaces.nsIFile);
    try {
      ifile.URL = URL2Validate;
      // nsIFile throws an exception if it's not a file url
    } catch (e) { ifile = null; }
    if (ifile)
    {
      URL2Validate = ifile.path;
      var vwin = window.open("http://validator.w3.org/file-upload.html",
                             "EditorValidate");
      // Window loads asynchronously, so pass control to the load listener:
      vwin.addEventListener("load", this.validateFilePageLoaded, false);
    }
    else
    {
      var vwin = window.open("http://validator.w3.org/",
                             "EditorValidate");
      // Window loads asynchronously, so pass control to the load listener:
      vwin.addEventListener("load", this.validateWebPageLoaded, false);
    }
  },
  validateFilePageLoaded: function(event)
  {
    event.target.forms[0].uploaded_file.value = URL2Validate;
  },
  validateWebPageLoaded: function(event)
  {
    event.target.forms[0].uri.value = URL2Validate;
  }
};

// Link checking.
// XXX THIS CODE IS WORK IN PROGRESS (only exposed in the debug menu).

// Variables used across all the links being checked:
const kIOSERVICE_CONTRACTID = "@mozilla.org/network/io-service;1";
const nsIIOService = Components.interfaces['nsIIOService'];
var gIOService = 0;
var gNumLinksToCheck = 0;     // The number of nsILinkCheckers
var gLinksBeingChecked;       // holds the array of nsILinkCheckers
var gNumLinksCalledBack = 0;
var gStartedAllChecks = false;
var gLinkCheckTimerID = 0;

function GetIOService()
{
  if (gIOService == 0)
    gIOService = Components.classes[kIOSERVICE_CONTRACTID].getService(nsIIOService);
}

function nsLinkCheckTimeOut()
{
  // We might have gotten here via a late timeout
  if (gNumLinksToCheck <= 0)
    return;
  dump("Timed out!  Heard from " + gNumLinksCalledBack + " of "
       + gNumLinksToCheck + "\n");
  for (var i=0; i < gNumLinksToCheck; ++i)
  {
    var linkChecker = gLinksBeingChecked[i].QueryInterface(Components.interfaces.nsIURIChecker);
    var status = linkChecker.status;
    if (status == 1)         // LINK_INVALID
      dump(">> " + linkChecker.name + " is broken\n");
    else if (status == 2)    // LINK_TIMED_OUT
      dump(">> " + linkChecker.name + " timed out\n");
    else if (status == 3)    // LINK_NOT_CHECKED
      dump(">> " + linkChecker.name + " not checked\n");
    else
      dump("   " + linkChecker.name + " OK!\n");
  }

  delete gLinksBeingChecked;  // This deletes/cancels everything
  gNumLinksToCheck = 0;
  gStartedAllChecks = false;
}

var nsCheckLinksCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell != null);
  },

  doCommand: function(aCommand)
  {
    //window.openDialog("chrome://editor/content/EdLinkCheck.xul", "_blank",
    //                  "chrome,close,titlebar", "");

    objects = window.editorShell.GetLinkedObjects();
    // Objects is an nsISupportsArray.
    gLinksBeingChecked = new Array;
    gNumLinksCalledBack = 0;
    gStartedAllChecks = false;
    gLinkCheckTimerID = setTimeout("nsLinkCheckTimeOut()", 5000);

    // We'll need the IO service if we don't already have one:
    //GetIOService();

    // Loop over the nodes that have links:
    for (gNumLinksToCheck = 0; gNumLinksToCheck < objects.Count();
         ++gNumLinksToCheck)
    {
      var refobj = objects.GetElementAt(gNumLinksToCheck).QueryInterface(Components.interfaces.nsIURIRefObject);
      // Loop over the links in this node:
      try {
        var uri;
        while (refobj && (uri = refobj.GetNextURI()))
        {
          // Use the real class in netlib:
          dump(gNumLinksToCheck + ": Trying to check " + uri + "\n");
          // Make a new nsIURIChecker
          gLinksBeingChecked[gNumLinksToCheck]
            = Components.classes["@mozilla.org/network/urichecker;1"]
                .createInstance()
                  .QueryInterface(Components.interfaces.nsIURIChecker);
          gLinksBeingChecked[gNumLinksToCheck].asyncCheckURI(uri, this);
        }
      } catch(e) {
        // NS_ERROR_NOT_AVAILABLE means the refobj was out of uris,
        // anything else means a real error that we should handle
        // (probably throw again).
        //dump("Exception: result = " + e.result + ", '" + e.message + "'\n");
        if (e.result != 2147746065)
          throw(e);
      }
    }
    // Done with the loop, now we can be prepared for the finish:
    gStartedAllChecks = true;
  },

  // urichecker requires that we have an OnStartRequest even tho it's a nop
  onStartRequest: function(request, ctxt) { },

  onStopRequest: function(request, ctxt, status)
  {
    var linkChecker = request.QueryInterface(Components.interfaces.nsIURIChecker);
    if (linkChecker)
    {
      ++gNumLinksCalledBack;
      if (gStartedAllChecks && gNumLinksCalledBack >= gNumLinksToCheck)
      {
        clearTimeout(gLinkCheckTimerID);
        nsLinkCheckTimeOut();
      }
    }
  }
};

//-----------------------------------------------------------------------------------
var nsImageCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
          try {
            var align = gPrefs.getIntPref("editor.hrule.align");
            if (align == 0 ) {
              hLine.setAttribute("align", "left");
            } else if (align == 2) {
              hLine.setAttribute("align", "right");
            }

            //Note: Default is center (don't write attribute)
  	  
            var width = gPrefs.getIntPref("editor.hrule.width");
            var percent = gPrefs.getBoolPref("editor.hrule.width_percent");
            if (percent)
              width = width +"%";

            hLine.setAttribute("width", width);

            var height = gPrefs.getIntPref("editor.hrule.height");
            hLine.setAttribute("size", String(height));

            var shading = gPrefs.getBoolPref("editor.hrule.shading");
            if (shading) {
              hLine.removeAttribute("noshade");
            } else {
              hLine.setAttribute("noshade", "noshade");
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.editorShell.InsertSource("<br>");
  }
};

//-----------------------------------------------------------------------------------
var nsInsertBreakAllCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.editorShell.InsertSource("<br clear='all'>");
  }
};

//-----------------------------------------------------------------------------------
var nsListPropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdPageProps.xul","_blank", "chrome,close,titlebar,modal", "");
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsObjectPropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    var isEnabled = false;
    if (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML())
    {
      isEnabled = (GetObjectForProperties() != null ||
                   window.editorShell.GetSelectedElement("href") != null);
    }
    return isEnabled;
  },
  doCommand: function(aCommand)
  {
    // Launch Object properties for appropriate selected element 
    var element = GetObjectForProperties();
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
        case 'th':
          EditorTableCellProperties();
          break;
        case 'ol':
        case 'ul':
        case 'dl':
          goDoCommand("cmd_listProperties");
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


//-----------------------------------------------------------------------------------
var nsSetSmiley =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return ( window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());

  },

  doCommand: function(aCommand)
  {
	
    var commandNode = document.getElementById(aCommand);
    var smileyCode = commandNode.getAttribute("state");

    var strSml;
    switch(smileyCode)
    {
        case ":-)": strSml="s1"; 
        break;
        case ":-(": strSml="s2";
        break;
        case ";-)": strSml="s3";
        break;
        case ":-P": strSml="s4";
        break;
        case ":-D": strSml="s5";
        break;
        case ":-[": strSml="s6";
        break;
        case ":-\\": strSml="s7";
        break;
        default:	strSml="";
        break;
    }

    try 
    {
      var selection = window.editorShell.editorSelection;

      if (!selection)
        return;
	
      var extElement = editorShell.CreateElementWithDefaults("span");
      if (!extElement)
        return;
	
      extElement.setAttribute("-moz-smiley", strSml);

	
      var intElement = editorShell.CreateElementWithDefaults("span");
      if (!intElement)
        return;

	  //just for mailnews, because of the way it removes HTML
      var smileButMenu = document.getElementById('smileButtonMenu');      
      if (smileButMenu.getAttribute("padwithspace"))
         smileyCode = " " + smileyCode + " ";

      var txtElement =  document.createTextNode(smileyCode);
      if (!txtElement)
		return;

      intElement.appendChild (txtElement);
      extElement.appendChild (intElement);


      editorShell.InsertElementAtSelection(extElement,true);
      window._content.focus();		

    } 
    catch (e) 
    {
        dump("Exception occured in smiley InsertElementAtSelection\n");
    }
	
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
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
    return IsEditorContentHTML(); //(window.editorShell && window.editorShell.documentEditable);
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditorContentHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditorContentHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditorContentHTML());
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
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    if (IsInTableCell())
      EditorTableCellProperties();
    else
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
    return window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML();
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
    var rows = GetNumberOfContiguousSelectedRows();
    // Delete at least one row
    if (rows == 0)
      rows = 1;

    try {
      window.editorShell.BeginBatchChanges();

      // Loop to delete all blocks of contiguous, selected rows
      while (rows)
      {
        window.editorShell.DeleteTableRow(rows);
        rows = GetNumberOfContiguousSelectedRows();
      }
      window.editorShell.EndBatchChanges();
    } catch(ex) {
      window.editorShell.EndBatchChanges();
    }
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
    var columns = GetNumberOfContiguousSelectedColumns();
    // Delete at least one column
    if (columns == 0)
      columns = 1;

    try {
      window.editorShell.BeginBatchChanges();

      // Loop to delete all blocks of contiguous, selected columns
      while (columns)
      {
        window.editorShell.DeleteTableColumn(columns);
        columns = GetNumberOfContiguousSelectedColumns();
      }
      window.editorShell.EndBatchChanges();
    } catch(ex) {
      window.editorShell.EndBatchChanges();
    }
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
    if (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML())
    {
      var tagNameObj = new Object;
      var countObj = new Object;
      var cell = window.editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);

      // We need a cell and either > 1 selected cell or a cell to the right
      //  (this cell may originate in a row spanned from above current row)
      // Note that editorShell returns "td" for "th" also.
      // (this is a pain! Editor and gecko use lowercase tagNames, JS uses uppercase!)
      if( cell && (tagNameObj.value == "td"))
      {
        // Selected cells
        if (countObj.value > 1) return true;

        var colSpan = cell.getAttribute("colspan");

        // getAttribute returns string, we need number
        // no attribute means colspan = 1
        if (!colSpan)
          colSpan = Number(1);
        else
          colSpan = Number(colSpan);

        // Test if cell exists to the right of current cell
        // (cells with 0 span should never have cells to the right
        //  if there is, user can select the 2 cells to join them)
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
    if (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML())
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
        var rowSpan = cell.getAttribute("rowspan");
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
    window._content.focus();
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

var nsBuildRecentPagesMenu =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;
  },
  doCommand: function(aCommand)
  {
    // In editor.js. True means save menu to prefs
    BuildRecentMenu(true);
  }
};

var nsConvertToTable =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    if (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML())
    {
    var selection = window.editorShell.editorSelection;

    if (selection && !selection.isCollapsed)
    {
      // Don't allow if table or cell is the selection
      var element = window.editorShell.GetSelectedElement("");

      if (element && (element.nodeName == "td" ||
                      element.nodeName == "table"))
        return false;

      // Selection start and end must be in the same cell
      //   in same cell or both are NOT in a cell
      if ( GetParentTableCell(selection.focusNode) !=
           GetParentTableCell(selection.anchorNode) )
        return false
      
      return true;
      }
    }
    return false;
  },
  doCommand: function(aCommand)
  {
    if (this.isCommandEnabled())
    {
      window.openDialog("chrome://editor/content/EdConvertToTable.xul","_blank", "chrome,close,titlebar,modal")
    }
    window._content.focus();
  }
};

