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
 *    Kathleen Brade (brade@netscape.com)
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
  controller.registerCommand("cmd_editLink",        nsEditLinkCommand);
  
  controller.registerCommand("cmd_form",          nsFormCommand);
  controller.registerCommand("cmd_inputtag",      nsInputTagCommand);
  controller.registerCommand("cmd_inputimage",    nsInputImageCommand);
  controller.registerCommand("cmd_textarea",      nsTextAreaCommand);
  controller.registerCommand("cmd_select",        nsSelectCommand);
  controller.registerCommand("cmd_button",        nsButtonCommand);
  controller.registerCommand("cmd_label",         nsLabelCommand);
  controller.registerCommand("cmd_fieldset",      nsFieldSetCommand);
  controller.registerCommand("cmd_isindex",       nsIsIndexCommand);
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
  commandManager.registerCommand("cmd_publish",        nsPublishCommand);
  commandManager.registerCommand("cmd_publishAs",      nsPublishAsCommand);
  commandManager.registerCommand("cmd_publishSettings",nsPublishSettingsCommand);
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
    fp.init(window, GetString("OpenHTMLFile"), nsIFilePicker.modeOpen);

    SetFilePickerDirectory(fp, "html");

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
     *  since file.URL will be "file:///" if no filename picked (Cancel button used)
     */
    if (fp.file && fp.file.path.length > 0) {
      SaveFilePickerDirectory(fp, "html");
      editPage(fp.fileURL.spec, window, false);
    }
  }
};

// ******* File output commands and utilities ******** //
//-----------------------------------------------------------------------------------
var nsSaveCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return window.editorShell && 
      (window.editorShell.documentModified || 
       IsUrlAboutBlank(GetDocumentUrl()) ||
       window.gHTMLSourceChanged);
  },
  
  doCommand: function(aCommand)
  {
    var result = false;
    if (window.editorShell)
    {
      // XXX Switching keybinding from Save to Publish isn't working now :(
      //     so do publishing if editing remote url
      var docUrl = GetDocumentUrl();
      var scheme = GetScheme(docUrl);
      if (scheme && scheme != "file")
        goDoCommand("cmd_publish");

      FinishHTMLSource();
      result = SaveDocument(IsUrlAboutBlank(docUrl), false, editorShell.contentsMIMEType);
      window._content.focus();
    }
    return result;
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
      var result = SaveDocument(true, false, editorShell.contentsMIMEType);
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
      var result = SaveDocument(true, true, "text/plain");
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
        window.ok = SaveDocument(true, true, "text/plain");
      }
      else
      {
        window.ok = SaveDocument(true, false, editorShell.contentsMIMEType);
      }
    }

    window._content.focus();
    return window.ok;
  }
};

var nsPublishCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return window.editorShell && 
      (window.editorShell.documentModified || 
       IsUrlAboutBlank(GetDocumentUrl()) ||
       window.gHTMLSourceChanged);
  },
  
  doCommand: function(aCommand)
  {
    if (window.editorShell)
    {
      var docUrl = GetDocumentUrl();
      var filename = GetFilename(docUrl);
      var publishData;
      if (filename)
      {
        // Try to get site data from the document url
        publishData = CreatePublishDataFromUrl(docUrl);

        // If none, use default publishing site
        //XXX Should we do this? Maybe bring up dialog instead?
        if (!publishData)
        {
         dump(" *** PUBLISHING TO DEFAULT SITE\n");
          publishData = GetPublishDataFromSiteName(GetDefaultPublishSiteName(), filename);
        }

        if (publishData)
        {
          FinishHTMLSource();
          return Publish(publishData);
        }
      }

      // User needs to supply a filename or we didn't find publish data above
      // Bring up the dialog via cmd_publishAs, 
      goDoCommand("cmd_publishAs")
      return true;
    }
    return false;
  }
}

var nsPublishAsCommand =
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

      window.ok = false;
      publishData = {};
      window.openDialog("chrome://editor/content/EditorPublish.xul","_blank", 
                        "chrome,close,titlebar,modal", "", "", publishData);
      window._content.focus();
      if (window.ok)
        return Publish(publishData);
    }
    return false;
  }
}

// ------- output utilites   ----- //

// returns a fileExtension string
function GetExtensionBasedOnMimeType(aMIMEType)
{
  try {
    var mimeService = null;
    mimeService = Components.classes["@mozilla.org/mime;1"].getService();
    mimeService = mimeService.QueryInterface(Components.interfaces.nsIMIMEService);
    if (!mimeService) return "";

    var mimeInfo = mimeService.GetFromMIMEType(aMIMEType);
    if (!mimeInfo) return "";

    var fileExtension = mimeInfo.primaryExtension;

    // the MIME service likes to give back ".htm" for text/html files,
    // so do a special-case fix here.
    if (fileExtension == "htm")
      fileExtension = "html";

    return fileExtension;
  }
  catch (e) {}
  return "";
}

function GetSuggestedFileName(aDocumentURLString, aMIMEType, aHTMLDoc)
{
  var extension = GetExtensionBasedOnMimeType(aMIMEType);
  if (extension)
    extension = "." + extension;

  // check for existing file name we can use
  if (aDocumentURLString.length >= 0 && !IsUrlAboutBlank(aDocumentURLString))
  {
    var docURI = null;
    try {
      docURI = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURI);
      docURI.spec = aDocumentURLString;
      var docURL = docURI.QueryInterface(Components.interfaces.nsIURL);

      // grab the file name
      var url = docURL.fileBaseName;
      if (url)
        return url+extension;
    } catch(e) {}
  } 

  // check if there is a title we can use
  var title = aHTMLDoc.title;
  if (title.length > 0) // we have a title; let's see if it's usable
  {
    // clean up the title to make it a usable filename
    title = title.replace(/\"/g, "");  // Strip out quote character: "
    title = TrimString(title); // trim whitespace from beginning and end
    title = title.replace(/[ \.\\@\/:]/g, "_");  //Replace "bad" filename characters with "_"
    if (title.length > 0)
      return title + extension;
  }

  // if we still don't have a file name, let's just go with "untitled"
  // shouldn't this come out of a string bundle? I'm shocked localizers haven't complained!
  return "untitled" + extension;
}

// returns file picker result
function PromptForSaveLocation(aDoSaveAsText, aEditorType, aMIMEType, ahtmlDocument, aDocumentURLString)
{
  var dialogResult = new Object;
  dialogResult.filepickerClick = nsIFilePicker.returnCancel;
  dialogResult.resultingURI = "";
  dialogResult.resultingLocalFile = null;

  var fp = null;
  try {
    fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  } catch (e) {}
  if (!fp) return dialogResult;

  // determine prompt string based on type of saving we'll do
  var promptString;
  if (aDoSaveAsText || aEditorType == "text")
    promptString = GetString("ExportToText");
  else
    promptString = GetString("SaveDocumentAs")

  fp.init(window, promptString, nsIFilePicker.modeSave);

  // Set filters according to the type of output
  if (aDoSaveAsText)
    fp.appendFilters(nsIFilePicker.filterText);
  else
    fp.appendFilters(nsIFilePicker.filterHTML);
  fp.appendFilters(nsIFilePicker.filterAll);

  // now let's actually set the filepicker's suggested filename
  var suggestedFileName = GetSuggestedFileName(aDocumentURLString, aMIMEType, ahtmlDocument);
  if (suggestedFileName)
    fp.defaultString = suggestedFileName;

  // set the file picker's current directory
  // assuming we have information needed (like prior saved location)
  try {
    var ioService = GetIOService();
    
    var isLocalFile = true;
    try {
      var docURI = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsIURI);
      docURI.spec = aDocumentURLString;
      isLocalFile = docURI.schemeIs("file");
    }
    catch (e) {}

    var parentLocation = null;
    if (isLocalFile)
    {
      var fileLocation = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsIFile);
      ioService.initFileFromURLSpec(fileLocation, aDocumentURLString);  // this asserts if url is not local
      parentLocation = fileLocation.parent;
    }
    if (parentLocation)
    {
      // Save current filepicker's default location
      if ("gFilePickerDirectory" in window)
        gFilePickerDirectory = fp.displayDirectory;

      fp.displayDirectory = parentLocation;
    }
    else
    {
      // Initialize to the last-used directory for the particular type (saved in prefs)
      SetFilePickerDirectory(fp, aEditorType);
    }
  }
  catch(e) {}

  dialogResult.filepickerClick = fp.show();
  if (dialogResult.filepickerClick != nsIFilePicker.returnCancel)
  {
    // reset urlstring to new save location
    dialogResult.resultingURIString = ioService.getURLSpecFromFile(fp.file);
    dialogResult.resultingLocalFile = fp.file;
    SaveFilePickerDirectory(fp, aEditorType);
  }
  else if ("gFilePickerDirectory" in window && gFilePickerDirectory)
    fp.displayDirectory = gFilePickerDirectory; 

  return dialogResult;
}

// returns a boolean (whether to continue (true) or not (false) because user canceled)
function PromptAndSetTitleIfNone(aHTMLDoc)
{
  if (!aHTMLDoc) throw NS_ERROR_NULL_POINTER;

  var title = aHTMLDoc.title;
  if (title.length > 0) // we have a title; no need to prompt!
    return true;

  var promptService = null;
  try {
    promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
  }
  catch (e) {}
  if (!promptService) return false;

  var result = {value:null};
  var captionStr = GetString("DocumentTitle");
  var msgStr = GetString("NeedDocTitle") + '\n' + GetString("DocTitleHelp");
  var confirmed = promptService.prompt(window, captionStr, msgStr, result, null, {value:0});
  if (confirmed && result.value && result.value != "")
    window.editorShell.SetDocumentTitle(result.value);

  return confirmed;
}

var gPersistObj;

// Don't forget to do these things after calling OutputFileWithPersistAPI:
//    window.editorShell.doAfterSave(doUpdateURLOnDocument, urlstring);  // we need to update the url before notifying listeners
//    if (!aSaveCopy && success)
//      window.editorShell.editor.resetModificationCount();
      // this should cause notification to listeners that document has changed

const webPersist = Components.interfaces.nsIWebBrowserPersist;
function OutputFileWithPersistAPI(editorDoc, aDestinationLocation, aRelatedFilesParentDir, aMimeType)
{
  gPersistObj = null;
  try {
    var imeEditor = window.editorShell.editor.QueryInterface(Components.interfaces.nsIEditorIMESupport);
    if (imeEditor)
      imeEditor.ForceCompositionEnd();
    } catch (e) {}

  var isLocalFile = false;
  try {
    var tmp1 = aDestinationLocation.QueryInterface(Components.interfaces.nsIFile);
    isLocalFile = true;
  } 
  catch (e) {
    try {
      var tmp = aDestinationLocation.QueryInterface(Components.interfaces.nsIURI);
      isLocalFile = tmp.schemeIs("file");
    }
    catch (e) {}
  }

  try {
    // we should supply a parent directory if/when we turn on functionality to save related documents
    var persistObj = Components.classes["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"].createInstance(webPersist);
    persistObj.progressListener = gEditorOutputProgressListener;
    
    var wrapColumn = GetWrapColumn();
    var outputFlags = GetOutputFlags(aMimeType, wrapColumn);

    // for 4.x parity as well as improving readability of file locally on server
    // this will always send crlf for upload (http/ftp)
    if (!isLocalFile) // if we aren't saving locally then send both cr and lf
      outputFlags |= webPersist.ENCODE_FLAGS_CR_LINEBREAKS | webPersist.ENCODE_FLAGS_LF_LINEBREAKS;

    // note: we always want to set the replace existing files flag since we have
    // already given user the chance to not replace an existing file (file picker)
    // or the user picked an option where the file is implicitly being replaced (save)
    persistObj.persistFlags = persistObj.persistFlags 
                            | webPersist.PERSIST_FLAGS_NO_BASE_TAG_MODIFICATIONS
                            | webPersist.PERSIST_FLAGS_REPLACE_EXISTING_FILES
                            | webPersist.PERSIST_FLAGS_DONT_FIXUP_LINKS
                            | webPersist.PERSIST_FLAGS_FIXUP_ORIGINAL_DOM;
    persistObj.saveDocument(editorDoc, aDestinationLocation, aRelatedFilesParentDir, 
                            aMimeType, outputFlags, wrapColumn);
    gPersistObj = persistObj;
  }
  catch(e) { dump("caught an error, bail\n"); return false; }

  return true;
}

// returns output flags based on mimetype, wrapCol and prefs
function GetOutputFlags(aMimeType, aWrapColumn)
{
  var outputFlags = webPersist.ENCODE_FLAGS_ENCODE_ENTITIES;
  if (aMimeType == "text/plain")
  {
    // When saving in "text/plain" format, always do formatting
    outputFlags |= webPersist.ENCODE_FLAGS_FORMATTED;
  }
  else
  {
    // Should we prettyprint? Check the pref
    try {
      var prefs = GetPrefs();
      if (prefs.getBoolPref("editor.prettyprint"))
        outputFlags |= webPersist.ENCODE_FLAGS_FORMATTED;
    }
    catch (e) {}
  }

  if (aWrapColumn > 0)
    outputFlags |= webPersist.ENCODE_FLAGS_WRAP;

  return outputFlags;
}

// returns number of column where to wrap
function GetWrapColumn()
{
  var wrapCol = 72;
  try {
    wrapCol = window.editorShell.editor.wrapWidth;
  }
  catch (e) {}

  return wrapCol;
}

function GetPromptService()
{
  var promptService;
  try {
    promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
  }
  catch (e) {}
  return promptService;
}

const gShowDebugOutputStateChange = false;
const gShowDebugOutputProgress = false;
const gShowDebugOutputLocationChange = false;
const gShowDebugOutputStatusChange = false;
const gShowDebugOutputSecurityChange = false;

var gEditorOutputProgressListener =
{
  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {
    // Use this to access onStateChange flags
    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    var requestSpec;
    try {
      var channel = aRequest.QueryInterface(Components.interfaces.nsIChannel);
      if (channel)
        requestSpec = StripUsernamePasswordFromURI(channel.URI);
    } catch (e) {
      if ( gShowDebugOutputStateChange)
        dump("***** onStateChange; NO REQUEST CHANNEL\n");
    }

    if (gShowDebugOutputStateChange)
    {
      dump("***** onStateChange request: " + requestSpec + "\n");
      dump("      state flags: ");

      if (aStateFlags & nsIWebProgressListener.STATE_START)
        dump(" STATE_START, ");
      if (aStateFlags & nsIWebProgressListener.STATE_STOP)
        dump(" STATE_STOP, ");
      if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK)
        dump(" STATE_IS_NETWORK ");

      dump("\n");
    }
    // Detect end of file upload of HTML file:
    if (gPublishData)
    {
      var pubSpec = gPublishData.publishUrl + gPublishData.docDir + gPublishData.filename;

      if ((aStateFlags & nsIWebProgressListener.STATE_STOP) &&
          (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK)
           && requestSpec && requestSpec == pubSpec)
      {
        // Get the new docUrl from the "browse location" in case "publish location" was FTP
        var urlstring = GetDocUrlFromPublishData(gPublishData);
        
        try {
          // check http channel for response: 200 range is ok; other ranges are not
          var httpChannel = aRequest.QueryInterface(Components.interfaces.nsIHttpChannel);
          var httpResponse = httpChannel.responseStatus;
          if (httpResponse < 200 || httpResponse >= 300)
            aStatus = httpResponse;   // not a real error but enough to pass check below
        } catch(e) {}

        if (aStatus)
        {
          // we should cancel the publish transaction here!

          // we should provide more meaningful errors (if possible)
          var saveDocStr = GetString("Publish");
          var failedStr = GetString("PublishFailed");
          AlertWithTitle(saveDocStr, failedStr);

          return;  // we don't want to change location or reset mod count, etc.
        }

        try {
          window.editorShell.doAfterSave(true, urlstring);  // we need to update the url before notifying listeners
          var editor = window.editorShell.editor.QueryInterface(Components.interfaces.nsIEditor);
          editor.resetModificationCount();
          // this should cause notification to listeners that doc has changed

          // Set UI based on whether we're editing a remote or local url
          SetSaveAndPublishUI(urlstring);
        } catch (e) {}
      }
    }
  },

  onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress,
                              aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
  {
    if (gShowDebugOutputProgress)
    {
      try {
      var channel = aRequest.QueryInterface(Components.interfaces.nsIChannel);
      dump("***** onProgressChange request: " + channel.URI.spec + "\n");
      }
      catch (e) {}
      dump("*****       self:  "+aCurSelfProgress+" / "+aMaxSelfProgress+"\n");
      dump("*****       total: "+aCurTotalProgress+" / "+aMaxTotalProgress+"\n\n");

      if (gPersistObj)
      {
        if(gPersistObj.currentState == gPersistObj.PERSIST_STATE_READY)
          dump(" Persister is ready to save data\n\n");
        else if(gPersistObj.currentState == gPersistObj.PERSIST_STATE_SAVING)
          dump(" Persister is saving data.\n\n");
        else if(gPersistObj.currentState == gPersistObj.PERSIST_STATE_FINISHED)
        {
          dump(" PERSISTER HAS FINISHED SAVING DATA\n\n\n");
        }
      }
    }
  },

  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
    if (gShowDebugOutputLocationChange)
    {
      dump("***** onLocationChange: "+aLocation.spec+"\n");
      var channel = aRequest.QueryInterface(Components.interfaces.nsIChannel);
      dump("*****          request: " + channel.URI.spec + "\n");
    }
  },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
    if (gShowDebugOutputStatusChange)
    {
      dump("***** onStatusChange: "+aMessage+"\n");
      try {
        var channel = aRequest.QueryInterface(Components.interfaces.nsIChannel);
        dump("*****        request: " + channel.URI.spec + "\n");
      }
      catch (e) { dump("          couldn't get request\n"); }

      if (aStatus == 2152398852)
        dump("*****        status is UNKNOWN_TYPE\n");
      else if (aStatus == 2152398853)
        dump("*****        status is DESTINATION_NOT_DIR\n");
      else if (aStatus == 2152398854)
        dump("*****        status is TARGET_DOES_NOT_EXIST\n");
      else if (aStatus == 2152398856)
        dump("*****        status is ALREADY_EXISTS\n");
      else if (aStatus == 2152398858)
        dump("*****        status is DISK_FULL\n");
      else if (aStatus == 2152398860)
        dump("*****        status is NOT_DIRECTORY\n");
      else if (aStatus == 2152398861)
        dump("*****        status is IS_DIRECTORY\n");
      else if (aStatus == 2152398862)
        dump("*****        status is IS_LOCKED\n");
      else if (aStatus == 2152398863)
        dump("*****        status is TOO_BIG\n");
      else if (aStatus == 2152398865)
        dump("*****        status is NAME_TOO_LONG\n");
      else if (aStatus == 2152398866)
        dump("*****        status is NOT_FOUND\n");
      else if (aStatus == 2152398867)
        dump("*****        status is READ_ONLY\n");
      else if (aStatus == 2152398868)
        dump("*****        status is DIR_NOT_EMPTY\n");
      else if (aStatus == 2152398869)
        dump("*****        status is ACCESS_DENIED\n");
      else
        dump("*****        status is " + aStatus + "\n");

      if (gPersistObj)
      {
        if(gPersistObj.currentState == gPersistObj.PERSIST_STATE_READY)
          dump(" Persister is ready to save data\n\n");
        else if(gPersistObj.currentState == gPersistObj.PERSIST_STATE_SAVING)
          dump(" Persister is saving data.\n\n");
        else if(gPersistObj.currentState == gPersistObj.PERSIST_STATE_FINISHED)
          dump(" PERSISTER HAS FINISHED SAVING DATA\n\n\n");
      }
    }
  },

  onSecurityChange : function(aWebProgress, aRequest, state)
  {
    if (gShowDebugOutputSecurityChange)
    {
      var channel = aRequest.QueryInterface(Components.interfaces.nsIChannel);
      dump("***** onSecurityChange request: " + channel.URI.spec + "\n");
    }
  },

  QueryInterface : function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener)
    || aIID.equals(Components.interfaces.nsISupports)
    || aIID.equals(Components.interfaces.nsISupportsWeakReference)
    || aIID.equals(Components.interfaces.nsIPrompt)
    || aIID.equals(Components.interfaces.nsIAuthPrompt))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

// nsIPrompt
  alert : function(dlgTitle, text)
  {
    AlertWithTitle(dlgTitle, text);
  },
  alertCheck : function(dialogTitle, text, checkBoxLabel, checkValue)
  {
    AlertWithTitle(dialogTitle, text);
    dump("***** warning: checkbox not shown to user\n");
  },
  confirm : function(dlgTitle, text)
  {
    return ConfirmWithTitle(dlgTitle, text, null, null);
  },
  confirmCheck : function(dlgTitle, text, checkBoxLabel, checkValue)
  {
    var promptServ = GetPromptService();
    if (!promptServ)
      return;

    promptServ.confirmEx(window, dlgTitle, text, nsIPromptService.STD_OK_CANCEL_BUTTONS,
                         "", "", "", checkBoxLabel, checkValue, outButtonPressed);
  },
  confirmEx : function(dlgTitle, text, btnFlags, btn0Title, btn1Title, btn2Title, checkBoxLabel, checkVal, outBtnPressed)
  {
    var promptServ = GetPromptService();
    if (!promptServ)
     return;

    promptServ.confirmEx(window, dlgTitle, text, btnFlags,
                        btn0Title, btn1Title, btn2Title,
                        checkBoxLabel, checkVal, outBtnPressed);
  },
  prompt : function(dlgTitle, text, inoutText, checkBoxLabel, checkValue)
  {
    var promptServ = GetPromptService();
    if (!promptServ)
     return false;

    return promptServ.prompt(window, dlgTitle, text, inoutText, checkBoxLabel, checkValue);
  },
  promptPassword : function(dlgTitle, text, password, checkBoxLabel, checkValue)
  {
    var promptServ = GetPromptService();
    if (!promptServ)
      return false;

    return promptServ.promptPassword(window, dlgTitle, text, password, checkBoxLabel, checkValue);
  },
  promptUsernameAndPassword : function(dlgTitle, text, login, pw, checkBoxLabel, checkValue)
  {
    var promptServ = GetPromptService();
    if (!promptServ)
      return false;

    return promptServ.promptUsernameAndPassword(window, dlgTitle, text, login, pw, checkBoxLabel, checkValue);
  },
  select : function(dlgTitle, text, count, selectList, outSelection)
  {
    var promptServ = GetPromptService();
    if (!promptServ)
      return false;

    return promptServ.select(window, dlgTitle, text, count, selectList, outSelection);
  },

// nsIAuthPrompt
  prompt : function(dlgTitle, text, pwrealm, savePW, defaultText, result)
  {
    dump("authprompt prompt! pwrealm="+pwrealm+"\n");
    var promptServ = GetPromptService();
    if (!promptServ)
      return false;

    var saveCheck = {value:savePW};
    return promptServ.prompt(window, dlgTitle, text, defaultText, pwrealm, saveCheck);
  },
  promptUsernameAndPassword : function(dlgTitle, text, pwrealm, savePW, user, pw)
  {
    dump("authprompt promptUsernameAndPassword!  "+dlgTitle+" "+text+", pwrealm="+pwrealm+"\n");
    var promptServ = GetPromptService();
    if (!promptServ)
      return false;

    var saveCheck = {value:savePW};
    // Initialize with user's previous preference for this site
    if (gPublishData)
      saveCheck.value = gPublishData.savePassword;

    var ret = promptServ.promptUsernameAndPassword(window, dlgTitle, text, user, pw, GetString("SavePassword"), saveCheck);

    //XXX We really shouldn't do this until publishing completed successfully
    if (ret && gPublishData)
      SaveUsernamePasswordFromPrompt(gPublishData, user.value, pw.value, saveCheck.value);

    return ret;
  },
  promptPassword : function(dlgTitle, text, pwrealm, savePW, pw)
  {
    dump("auth promptPassword!  "+dlgTitle+" "+text+", pwrealm="+pwrealm+"\n");
    var promptServ = GetPromptService();
    if (!promptServ)
      return false;

    var saveCheck = {value:savePW};
    // Initialize with user's previous preference for this site
    if (gPublishData)
      saveCheck.value = gPublishData.savePassword;

    var ret = promptServ.promptPassword(window, dlgTitle, text, pw, GetString("SavePassword"), saveCheck);

    //XXX We really shouldn't do this until publishing completed successfully
    if (ret && gPublishData)
      SaveUsernamePasswordFromPrompt(gPublishData, gPublishData.username, pw.value, saveCheck.value);

    return ret;
  }
}

// Save any data that the user supplied in a prompt dialog
function SaveUsernamePasswordFromPrompt(publishData, username, password, savePassword)
{
  if (!publishData || !username)
    return;

  var usernameChanged = gPublishData.username != username;
  var savePasswordChanged = gPublishData.savePassword != savePassword;
  publishData.username = username;
  publishData.password = password;
  publishData.savePassword = savePassword;
  
  if (usernameChanged || savePasswordChanged)
  {
    // A publishing pref item was changed (this will also save the password)
    SavePublishDataToPrefs(publishData);
  }
  else if (savePassword)
  {
    // Publish pref data is correct.
    // Probably user error in publish dialog, so save just password
    SavePassword(publishData);
  }
}

// throws an error or returns true if user attempted save; false if user canceled save
function SaveDocument(aSaveAs, aSaveCopy, aMimeType)
{
  if (!aMimeType || aMimeType == "" || !window.editorShell)
    throw NS_ERROR_NOT_INITIALIZED;

  var editorDoc = window.editorShell.editorDocument;
  if (!editorDoc)
    throw NS_ERROR_NOT_INITIALIZED;

  // if we don't have the right editor type bail (we handle text and html)
  var editorType = window.editorShell.editorType;
//  var isMailType = (editorType == "textmail" || editorType == "htmlmail")
  if (editorType != "text" && editorType != "html" && editorType != "htmlmail")
    throw NS_ERROR_NOT_IMPLEMENTED;

  var saveAsTextFile = window.editorShell.isSupportedTextType(aMimeType);

  // check if the file is to be saved in a format we don't understand; if so, bail
  if (aMimeType != "text/html" && !saveAsTextFile)
    throw NS_ERROR_NOT_IMPLEMENTED;

  if (saveAsTextFile)
    aMimeType = "text/plain";

  var urlstring = GetDocumentUrl();
  var mustShowFileDialog = (aSaveAs || IsUrlAboutBlank(urlstring) || (urlstring == ""));
  var replacing = !aSaveAs;
  var titleChanged = false;
  var doUpdateURL = false;
  var tempLocalFile = null;

  if (mustShowFileDialog)
  {
	  try {
	    var domhtmldoc = editorDoc.QueryInterface(Components.interfaces.nsIDOMHTMLDocument);

	    // Prompt for title if we are saving to HTML
	    if (!saveAsTextFile && (editorType == "html"))
	    {
	      var userContinuing = PromptAndSetTitleIfNone(domhtmldoc); // not cancel
	      if (!userContinuing)
	        return false;
	    }

	    var dialogResult = PromptForSaveLocation(saveAsTextFile, editorType, aMimeType, domhtmldoc, urlstring);
	    if (dialogResult.filepickerClick == nsIFilePicker.returnCancel)
	      return false;

	    replacing = (dialogResult.filepickerClick == nsIFilePicker.returnReplace);
	    urlstring = dialogResult.resultingURIString;
	    tempLocalFile = dialogResult.resultingLocalFile;
 
      // update the new URL for the webshell unless we are saving a copy
      if (!aSaveCopy)
        doUpdateURL = true;
   } catch (e) {  return false; }
  } // mustShowFileDialog

  var success = true;
  try {
    // if somehow we didn't get a local file but we did get a uri, 
    // attempt to create the localfile if it's a "file" url
    var docURI;
    if (!tempLocalFile)
    {
      docURI = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURI);
      docURI.spec = urlstring;
      
      if (docURI.schemeIs("file"))
      {
        tempLocalFile = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsILocalFile);
        var ioService = GetIOService();
        ioService.initFileFromURLSpec(tempLocalFile, urlstring);
      }
    }

    // this is the location where the related files will go
    var parentDir;
    try {
      if (tempLocalFile)
      {
        if (!aSaveAs)         // we are saving an existing local file
          parentDir = null;   // don't change links or move files
        else
        {
          // if we are saving to the same parent directory, don't set parentDir
          // grab old location, chop off file
          // grab new location, chop off file, compare
          var oldLocation = GetDocumentURI(editorDoc);
          var oldLocationLastSlash = oldLocation.lastIndexOf("\/");
          if (oldLocationLastSlash != -1)
            oldLocation = oldLocation.slice(0, oldLocationLastSlash);

          var curParentString = urlstring;
          var newLocationLastSlash = curParentString.lastIndexOf("\/");
          if (newLocationLastSlash != -1)
            curParentString = curParentString.slice(0, newLocationLastSlash);
          if (oldLocation == curParentString || IsUrlAboutBlank(oldLocation))
            parentDir = null;
          else
            parentDir = tempLocalFile.parent;  // this is wrong if parent is the root!
        }
      }
      else
      {
        var lastSlash = urlstring.lastIndexOf("\/");
        if (lastSlash != -1)
        {
          var parentDirString = urlstring.slice(0, lastSlash + 1);  // include last slash
          parentDir = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURI);
          parentDir.spec = parentDirString;
        }
        else
          parentDir = null;
      }
    } catch(e) { parentDir = null; }

    var destinationLocation;
    if (tempLocalFile)
      destinationLocation = tempLocalFile;
    else
      destinationLocation = docURI;

    success = OutputFileWithPersistAPI(editorDoc, destinationLocation, parentDir, aMimeType);
  }
  catch (e)
  {
    success = false;
  }

  if (success)
  {
    try {
      window.editorShell.doAfterSave(doUpdateURL, urlstring);  // we need to update the url before notifying listeners
      if (!aSaveCopy)
        window.editorShell.editor.resetModificationCount();
      // this should cause notification to listeners that document has changed

      // Set UI based on whether we're editing a remote or local url
      SetSaveAndPublishUI(urlstring);
    } catch (e) {}
  }
  else
  {
    var saveDocStr = GetString("SaveDocument");
    var failedStr = GetString("SaveFileFailed");
    AlertWithTitle(saveDocStr, failedStr);
  }
  return success;
}


//-------------------------------  Publishing
var gPublishData;

function Publish(publishData)
{
  if (!publishData)
    return false;

  var docURI = CreateURIFromPublishData(publishData, true);
  if (!docURI)
    return false;

  // Set data in global for username password requests
  //  and to do "post saving" actions after monitoring nsIWebProgressListener messages
  //  and we are sure file transfer was successful
  gPublishData = publishData;

  var otherFilesURI = CreateURIFromPublishData(publishData, false);
  var success = OutputFileWithPersistAPI(window.editorShell.editorDocument, 
                                         docURI, otherFilesURI, window.editorShell.contentsMIMEType);

  return success;
}

// Create a nsIURI object filled in with all required publishing info
function CreateURIFromPublishData(publishData, doDocUri)
{
  if (!publishData || !publishData.publishUrl)
    return null;

  var URI;
  try {
    URI = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURI);

    if (!URI)
      return null;

    var spec = publishData.publishUrl;
    if (doDocUri)
      spec += FormatDirForPublishing(publishData.docDir) + publishData.filename; 
    else
      spec += FormatDirForPublishing(publishData.otherDir);

    URI.spec = spec;

    if (publishData.username)
      URI.username = publishData.username;
    if (publishData.password)
      URI.password = publishData.password;
  }
  catch (e) {}

  return URI;
}

// Resolve the correct "http:" document URL when publishing via ftp
function GetDocUrlFromPublishData(publishData)
{
  if (!publishData || !publishData.filename || !publishData.publishUrl)
    return "";

  // If user was previously editing an "ftp" url, then keep that as the new scheme
  var url;
  var docScheme = GetScheme(GetDocumentUrl());

  if (docScheme == "ftp" || !publishData.browseUrl)
    url = publishData.publishUrl;
  else
    url = publishData.browseUrl;
  
  url += FormatDirForPublishing(publishData.docDir) + publishData.filename;

  return url;
}

// Depending on editing local vs. remote files:
//   1. Switch the "Save" and "Publish" buttons on toolbars,
//   2. Hide "Save" menuitem if editing remote
//   3. Shift accel+S keybinding to Save or Publish commands
// Note: A new, unsaved file is treated as a local file
//     (XXX Have a pref to treat as remote for user's who mostly edit remote?)
function SetSaveAndPublishUI(urlstring)
{
  // Associate the "save" keybinding with Save for local files, 
  //   or with Publish for remote files
  var scheme = GetScheme(urlstring);
  var menuItem1;
  var menuItem2;
  var saveButton = document.getElementById("saveButton");
  var publishButton = document.getElementById("publishButton");
  var command;

  if (!scheme || scheme == "file")
  {
    // Editing a new or local file
    menuItem1 = document.getElementById("publishMenuitem");
    menuItem2 = document.getElementById("saveMenuitem");
    command = "cmd_save";

    // Hide "Publish". Show "Save" toolbar and menu items
    SetElementHidden(publishButton, true);
    SetElementHidden(saveButton, false);
  }
  else
  {
    // Editing a remote file
    menuItem1 = document.getElementById("saveMenuitem");
    menuItem2 = document.getElementById("publishMenuitem");
    command = "cmd_publish";

    // Hide "Save", show "Publish" toolbar and menuitems
    SetElementHidden(saveButton, true);
    SetElementHidden(publishButton, false);
  }

  SetElementHidden(menuItem1, true);
  SetElementHidden(menuItem2, false);

  var key = document.getElementById("savekb");
  if (key && command)
    key.setAttribute("observes", command);

  if (menuItem1 && menuItem2)
  {
    menuItem1.removeAttribute("key");
    menuItem2.setAttribute("key","savekb");
  }
}

// ****** end of save / publish **********//

//-----------------------------------------------------------------------------------
var nsPublishSettingsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },

  doCommand: function(aCommand)
  {
    if (window.editorShell)
    {
      // Launch Publish Settings dialog

      window.ok = window.openDialog("chrome://editor/content/EditorPublishSettings.xul","_blank", "chrome,close,titlebar,modal", "");
      window._content.focus();
      return window.ok;
    }
    return false;
  }
}

//-----------------------------------------------------------------------------------
var nsRevertCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && 
            window.editorShell.documentModified &&
            !IsUrlAboutBlank(GetDocumentUrl()));
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
        title = GetString("untitled");

      var msg = GetString("AbandonChanges").replace(/%title%/,title);

      promptService.confirmEx(window, GetString("RevertCaption"), msg,
  						      (promptService.BUTTON_TITLE_REVERT * promptService.BUTTON_POS_0) +
  						      (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1),
  						      null, null, null, null, {value:0}, result);

      // Reload page if first button (Revert) was pressed
      if(result.value == 0)
      {
        FinishHTMLSource();
        window.editorShell.LoadUrl(GetDocumentUrl());
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
  if (CheckAndSaveDocument(GetString("BeforeClosing"), true)) 
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
    if (!CheckAndSaveDocument(GetString("BeforePreview"), DocumentHasBeenSaved()))
	    return;

    // Check if we saved again just in case?
	  if (DocumentHasBeenSaved())
    {
      var browser;
      try {
        // Find a browser with this URL
        var windowManager = Components.classes["@mozilla.org/rdf/datasource;1?name=window-mediator"].getService();
        var windowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator);
        var enumerator = windowManagerInterface.getEnumerator("navigator:browser");

        while ( enumerator.hasMoreElements() )
        {
          browser = windowManagerInterface.convertISupportsToDOMWindow( enumerator.getNext() );
          if ( browser && (window._content.location.href == browser._content.location.href))
            break;

          browser = null;
        }
      }
      catch (ex) {}

      // If none found, open a new browser
      if (!browser)
        browser = window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", window._content.location);

      try {
        if (browser)
        {
          // Be sure browser contains real source content, not cached
          // setTimeout is needed because the "browser" created by openDialog 
          //    needs time to finish else BrowserReloadSkipCache doesn't exist
          setTimeout( function(browser) { browser.BrowserReloadSkipCache(); }, 0, browser );
          browser.focus();
        }
      } catch (ex) {}
    }
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
    if (!CheckAndSaveDocument(GetString("SendPageReason"), DocumentHasBeenSaved()))
	    return;

    // Check if we saved again just in case?
	  if (DocumentHasBeenSaved())
    {
      // Launch Messenger Composer window with current page as contents
      var pageTitle = window.editorShell.editorDocument.title;
      var pageUrl = GetDocumentUrl();
      try
      {
        openComposeWindow(pageUrl, pageTitle);        
      } catch (ex) { dump("Cannot Send Page: " + ex + "\n"); }
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
    try {
      window.editorShell.Print();
    } catch (e) {}
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
    return ((window.editorShell));
  },

  doCommand: function(aCommand)
  {
    var prefs = GetPrefs();
    var newfind;
    if (prefs) {
      try {
        newfind = prefs.getBoolPref("editor.new_find");
      }
      catch (ex) {
        newfind = false;
      }
    }
    
    if (newfind)
    {
      try {
        window.openDialog("chrome://editor/content/EdReplace.xul", "_blank",
                          "chrome,dependent", "");
      }
      catch(ex) {
        dump("*** Exception: couldn't open Replace Dialog\n");
      }
      window._content.focus();
    }
    else {
      window.editorShell.Replace();
    }
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
    return (window.editorShell && window.editorShell.documentEditable && 
            !IsInHTMLSourceMode() && IsSpellCheckerInstalled());
  },

  doCommand: function(aCommand)
  {
    window.cancelSendMessage = false;
    try {
      window.openDialog("chrome://editor/content/EdSpellCheck.xul", "_blank",
              "chrome,close,titlebar,modal", false);
    }
    catch(ex) {}
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
      if (!CheckAndSaveDocument(GetString("BeforeValidate"),
                                false))
        return;

      // Check if we saved again just in case?
      if (!DocumentHasBeenSaved())    // user hit cancel?
        return;
    }

    URL2Validate = GetDocumentUrl();
    // See if it's a file:
    var ifile = Components.classes["@mozilla.org/file/local;1"].createInstance().QueryInterface(Components.interfaces.nsIFile);
    try {
      var ioService = GetIOService();
      ioService.initFileFromURLSpec(ifile, URL2Validate);
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
      var vwin2 = window.open("http://validator.w3.org/",
                             "EditorValidate");
      // Window loads asynchronously, so pass control to the load listener:
      vwin2.addEventListener("load", this.validateWebPageLoaded, false);
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

var nsCheckLinksCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell != null);
  },

  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdLinkChecker.xul","_blank", "chrome,close,titlebar,modal");
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsFormCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdFormProps.xul", "_blank", "chrome,close,titlebar,modal");
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsInputTagCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdInputProps.xul", "_blank", "chrome,close,titlebar,modal");
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsInputImageCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdInputImage.xul", "_blank", "chrome,close,titlebar,modal");
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsTextAreaCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdTextAreaProps.xul", "_blank", "chrome,close,titlebar,modal");
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsSelectCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdSelectProps.xul", "_blank", "chrome,close,titlebar,modal");
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsButtonCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    // && !editorShell.editorSelection.isCollapsed()?
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdButtonProps.xul", "_blank", "chrome,close,titlebar,modal");
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsLabelCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    var tagName = "label";
    var labelElement = editorShell.GetSelectedElement(tagName);
    if (!labelElement)
      labelElement = editorShell.GetElementOrParentByTagName(tagName, editorShell.editorSelection.anchorNode);
    if (!labelElement)
      labelElement = editorShell.GetElementOrParentByTagName(tagName, editorShell.editorSelection.focusNode);
    if (labelElement) {
      // We only open the dialog for an existing label
      window.openDialog("chrome://editor/content/EdLabelProps.xul", "_blank", "chrome,close,titlebar,modal", labelElement);
      window._content.focus();
    } else {
      editorShell.SetTextProperty(tagName, "", "");
    }
  }
};

//-----------------------------------------------------------------------------------
var nsFieldSetCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdFieldSetProps.xul", "_blank", "chrome,close,titlebar,modal");
    window._content.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsIsIndexCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    var isindexElement = editorShell.CreateElementWithDefaults("isindex");
    isindexElement.setAttribute("prompt", editorShell.GetContentsAs("text/plain", 1)); // OutputSelectionOnly
    editorShell.InsertElementAtSelection(isindexElement, true);
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
        var prefs = GetPrefs();
        if (prefs) {
          try {
            var align = prefs.getIntPref("editor.hrule.align");
            if (align == 0 ) {
              hLine.setAttribute("align", "left");
            } else if (align == 2) {
              hLine.setAttribute("align", "right");
            }

            //Note: Default is center (don't write attribute)
  	  
            var width = prefs.getIntPref("editor.hrule.width");
            var percent = prefs.getBoolPref("editor.hrule.width_percent");
            if (percent)
              width = width +"%";

            hLine.setAttribute("width", width);

            var height = prefs.getIntPref("editor.hrule.height");
            hLine.setAttribute("size", String(height));

            var shading = prefs.getBoolPref("editor.hrule.shading");
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
        case 'form':
          goDoCommand("cmd_form");
          break;
        case 'input':
          var type = element.getAttribute("type");
          if (type && type.toLowerCase() == "image")
            goDoCommand("cmd_inputimage");
          else
            goDoCommand("cmd_inputtag");
          break;
        case 'textarea':
          goDoCommand("cmd_textarea");
          break;
        case 'select':
          goDoCommand("cmd_select");
          break;
        case 'button':
          goDoCommand("cmd_button");
          break;
        case 'label':
          goDoCommand("cmd_label");
          break;
        case 'fieldset':
          goDoCommand("cmd_fieldset");
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
        case 'li':
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
    UpdateDefaultColors(); 
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
var nsEditLinkCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    // Not really used -- this command is only in context menu, and we do enabling there
    return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML());
  },
  doCommand: function(aCommand)
  {
    var element = window.editorShell.GetSelectedElement("href");
    if (element)
      editPage(element.href, window, false);

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

