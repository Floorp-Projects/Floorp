/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

/**
 * interfaces
 */
var nsIMsgCompDeliverMode = Components.interfaces.nsIMsgCompDeliverMode;
var nsIMsgCompSendFormat = Components.interfaces.nsIMsgCompSendFormat;
var nsIMsgCompConvertible = Components.interfaces.nsIMsgCompConvertible;
var nsIMsgCompType = Components.interfaces.nsIMsgCompType;
var nsIMsgCompFormat = Components.interfaces.nsIMsgCompFormat;
var nsIAbPreferMailFormat = Components.interfaces.nsIAbPreferMailFormat;
var nsIPlaintextEditorMail = Components.interfaces.nsIPlaintextEditor;


/**
 * In order to distinguish clearly globals that are initialized once when js load (static globals) and those that need to be 
 * initialize every time a compose window open (globals), I (ducarroz) have decided to prefix by s... the static one and
 * by g... the other one. Please try to continue and repect this rule in the future. Thanks.
 */
/**
 * static globals, need to be initialized only once
 */
var sMsgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService(Components.interfaces.nsIMsgComposeService);
var sComposeMsgsBundle = document.getElementById("bundle_composeMsgs");

var sPrefs = null;
var sPrefBranchInternal = null;
var sOther_headers = "";

/* Create message window object. This is use by mail-offline.js and therefore should not be renamed. We need to avoid doing 
   this kind of cross file global stuff in the future and instead pass this object as parameter when needed by function
   in the other js file.
*/
var msgWindow = Components.classes["@mozilla.org/messenger/msgwindow;1"].createInstance();


/**
 * Global variables, need to be re-initialized every time mostly because we need to release them when the window close
 */
var gHideMenus;
var gMsgCompose;
var gAccountManager;
var gIOService;
var gPromptService;
var gLDAPPrefsService;
var gWindowLocked;
var gContentChanged;
var gCurrentIdentity;
var defaultSaveOperation;
var gSendOrSaveOperationInProgress;
var gCloseWindowAfterSave;
var gIsOffline;
var gSessionAdded;
var gCurrentAutocompleteDirectory;
var gAutocompleteSession;
var gSetupLdapAutocomplete;
var gLDAPSession;
var gSavedSendNowKey;
var gSendFormat;
var gLogComposePerformance;

var gMsgIdentityElement;
var gMsgAddressingWidgetTreeElement;
var gMsgSubjectElement;
var gMsgAttachmentElement;
var gMsgBodyFrame;
var gMsgHeadersToolbarElement;

// i18n globals
var gCurrentMailSendCharset;
var gSendDefaultCharset;
var gCharsetTitle;
var gCharsetConvertManager;

var gLastElementToHaveFocus;  
var gSuppressCommandUpdating;
var gReceiptOptionChanged;

var gMailSession;

const kComposeAttachDirPrefName = "mail.compose.attach.dir";

function InitializeGlobalVariables()
{
  gAccountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
  gIOService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
  gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  
  //This migrates the LDAPServer Preferences from 4.x to mozilla format.
  try {
      gLDAPPrefsService = Components.classes["@mozilla.org/ldapprefs-service;1"].getService();       
      gLDAPPrefsService = gLDAPPrefsService.QueryInterface( Components.interfaces.nsILDAPPrefsService);                  
  } catch (ex) {dump ("ERROR: Cannot get the LDAP service\n" + ex + "\n");}

  gMsgCompose = null;
  gWindowLocked = false;
  gContentChanged = false;
  gCurrentIdentity = null;
  defaultSaveOperation = "draft";
  gSendOrSaveOperationInProgress = false;
  gCloseWindowAfterSave = false;
  gIsOffline = gIOService.offline;
  gSessionAdded = false;
  gCurrentAutocompleteDirectory = null;
  gAutocompleteSession = null;
  gSetupLdapAutocomplete = false;
  gLDAPSession = null;
  gSavedSendNowKey = null;
  gSendFormat = nsIMsgCompSendFormat.AskUser;
  gCurrentMailSendCharset = null;
  gSendDefaultCharset = null;
  gCharsetTitle = null;
  gCharsetConvertManager = Components.classes['@mozilla.org/charset-converter-manager;1'].getService(Components.interfaces.nsICharsetConverterManager2);
  gMailSession = Components.classes["@mozilla.org/messenger/services/session;1"].getService(Components.interfaces.nsIMsgMailSession);
  gHideMenus = false;
  // We are storing the value of the bool logComposePerformance inorder to avoid logging unnecessarily.
  if (sMsgComposeService)
    gLogComposePerformance = sMsgComposeService.logComposePerformance;

  gLastElementToHaveFocus = null;  
  gSuppressCommandUpdating = false;
  gReceiptOptionChanged = false;
}
InitializeGlobalVariables();

function ReleaseGlobalVariables()
{
  gAccountManager = null;
  gIOService = null;
  gPromptService = null;
  gLDAPPrefsService = null;
  gCurrentIdentity = null;
  gCurrentAutocompleteDirectory = null;
  gAutocompleteSession = null;
  gLDAPSession = null;
  gCharsetConvertManager = null;
  gMsgCompose = null;
  gMailSession = null;
}

function disableEditableFields()
{
  editorShell.editor.flags |= nsIPlaintextEditorMail.eEditorReadonlyMask;
  var disableElements = document.getElementsByAttribute("disableonsend", "true");
  for (i=0;i<disableElements.length;i++)
  {
    disableElements[i].setAttribute('disabled', 'true');
  }
}

function enableEditableFields()
{
  editorShell.editor.flags &= ~nsIPlaintextEditorMail.eEditorReadonlyMask;
  var enableElements = document.getElementsByAttribute("disableonsend", "true");
  for (i=0;i<enableElements.length;i++)
  {
    enableElements[i].removeAttribute('disabled');
  }
}

var gComposeRecyclingListener = {
  onClose: function() {
    //Reset recipients and attachments
    awResetAllRows();
    RemoveAllAttachments();

	  //We need to clear the identity popup menu in case the user will change them. It will be rebuilded later in ComposeStartup
    ClearIdentityListPopup(document.getElementById("msgIdentityPopup"));
 
    //Clear the subject
    document.getElementById("msgSubject").value = "";
    SetComposeWindowTitle();

    SetContentAndBodyAsUnmodified();
    disableEditableFields();
    ReleaseGlobalVariables();

    //Reset Boxes size    
    document.getElementById("headers-box").removeAttribute("height");
    document.getElementById("appcontent").removeAttribute("height");
    document.getElementById("addresses-box").removeAttribute("width");
    document.getElementById("attachments-box").removeAttribute("width");

    //Reset menu options
    document.getElementById("format_auto").setAttribute("checked", "true");
    document.getElementById("priority_normal").setAttribute("checked", "true");

    //Reset toolbars that could be hidden
    if (gHideMenus) {
      document.getElementById("formatMenu").hidden = false;
      document.getElementById("insertMenu").hidden = false;
      var showFormat = document.getElementById("menu_showFormatToolbar")
      showFormat.hidden = false;
      if (showFormat.getAttribute("checked") == "true")
        document.getElementById("FormatToolbar").hidden = false;
    }

    //Reset editor
    EditorResetFontAndColorAttributes();
    EditorCleanup();

    //Release the nsIMsgComposeParams object
    if (window.arguments && window.arguments[0])
      window.arguments[0] = null;

    var event = document.createEvent('Events');
    event.initEvent('compose-window-close', false, true);
    document.getElementById("msgcomposeWindow").dispatchEvent(event);
	},

	onReopen: function(params) {
    //Reset focus to avoid undesirable visual effect when reopening the winodw
    var identityElement = document.getElementById("msgIdentity");
    if (identityElement)
      identityElement.focus();

    InitializeGlobalVariables();
    window.editorShell.contentWindow.focus();
    enableEditableFields();
    ComposeStartup(true, params);

    var event = document.createEvent('Events');
    event.initEvent('compose-window-reopen', false, true);
    document.getElementById("msgcomposeWindow").dispatchEvent(event);
	}
};

var stateListener = {
  NotifyComposeFieldsReady: function() {
    ComposeFieldsReady();
  },

  ComposeProcessDone: function(aResult) {
    gWindowLocked = false;
    CommandUpdate_MsgCompose();
    enableEditableFields();

    if (aResult== Components.results.NS_OK)
    {
      SetContentAndBodyAsUnmodified();
     
      if (gCloseWindowAfterSave)
      {
        // Notify the SendListener that Send has been aborted and Stopped
        if (gMsgCompose)
        {
          var externalListener = gMsgCompose.getExternalSendListener();
          if (externalListener)
          {
              externalListener.onSendNotPerformed(null, Components.results.NS_ERROR_ABORT);
          }
        }
        MsgComposeCloseWindow(true);
    }
    }
   
    gCloseWindowAfterSave = false;
  },

  SaveInFolderDone: function(folderURI) {
    DisplaySaveFolderDlg(folderURI);
  }
};

// all progress notifications are done through the nsIWebProgressListener implementation...
var progressListener = {
    onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus)
    {
      if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_START)
      {
        document.getElementById('compose-progressmeter').setAttribute( "mode", "undetermined" );
      }
      
      if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP)
      {
        gSendOrSaveOperationInProgress = false;
        document.getElementById('compose-progressmeter').setAttribute( "mode", "normal" );
        document.getElementById('compose-progressmeter').setAttribute( "value", 0 );
        document.getElementById('statusText').setAttribute('label', '');
      }
    },
    
    onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
    {
      // Calculate percentage.
      var percent;
      if ( aMaxTotalProgress > 0 ) 
      {
        percent = parseInt( (aCurTotalProgress*100)/aMaxTotalProgress + .5 );
        if ( percent > 100 )
          percent = 100;
        
        document.getElementById('compose-progressmeter').removeAttribute("mode");
        
        // Advance progress meter.
        document.getElementById('compose-progressmeter').setAttribute( "value", percent );
      } 
      else 
      {
        // Progress meter should be barber-pole in this case.
        document.getElementById('compose-progressmeter').setAttribute( "mode", "undetermined" );
      }
    },

    onLocationChange: function(aWebProgress, aRequest, aLocation)
    {
      // we can ignore this notification
    },

    onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage)
    {
      // Looks like it's possible that we get call while the document has been already delete!
      // therefore we need to protect ourself by using try/catch
      try {
        statusText = document.getElementById("statusText");
        if (statusText)
          statusText.setAttribute("label", aMessage);
      } catch (ex) {};
    },

    onSecurityChange: function(aWebProgress, aRequest, state)
    {
      // we can ignore this notification
    },

    QueryInterface : function(iid)
    {
      if (iid.equals(Components.interfaces.nsIWebProgressListener) ||
          iid.equals(Components.interfaces.nsISupportsWeakReference) ||
          iid.equals(Components.interfaces.nsISupports))
        return this;
     
      throw Components.results.NS_NOINTERFACE;
    }
};

var defaultController =
{
  supportsCommand: function(command)
  {
    switch (command)
    {
      //File Menu
      case "cmd_attachFile":
      case "cmd_attachPage":
      case "cmd_close":
      case "cmd_saveDefault":
      case "cmd_saveAsFile":
      case "cmd_saveAsDraft":
      case "cmd_saveAsTemplate":
      case "cmd_sendButton":
      case "cmd_sendNow":
      case "cmd_sendWithCheck":
      case "cmd_sendLater":
      case "cmd_printSetup":
      case "cmd_print":
      case "cmd_quit":

      //Edit Menu
      case "cmd_pasteQuote":
      case "cmd_delete":
      case "cmd_selectAll":
      case "cmd_find":
      case "cmd_findNext":
      case "cmd_account":
      case "cmd_preferences":

      //View Menu
      case "cmd_showComposeToolbar":
      case "cmd_showFormatToolbar":

      //Insert Menu
      case "cmd_renderedHTMLEnabler":
      case "cmd_insert":
      case "cmd_link":
      case "cmd_anchor":
      case "cmd_image":
      case "cmd_hline":
      case "cmd_table":
      case "cmd_insertHTML":
      case "cmd_insertChars":
      case "cmd_insertBreak":
      case "cmd_insertBreakAll":

      //Format Menu
      case "cmd_decreaseFont":
      case "cmd_increaseFont":
      case "cmd_bold":
      case "cmd_italic":
      case "cmd_underline":
      case "cmd_strikethrough":
      case "cmd_superscript":
      case "cmd_subscript":
      case "cmd_nobreak":
      case "cmd_em":
      case "cmd_strong":
      case "cmd_cite":
      case "cmd_abbr":
      case "cmd_acronym":
      case "cmd_code":
      case "cmd_samp":
      case "cmd_var":
      case "cmd_removeList":
      case "cmd_ul":
      case "cmd_ol":
      case "cmd_dt":
      case "cmd_dd":
      case "cmd_listProperties":
      case "cmd_indent":
      case "cmd_outdent":
      case "cmd_objectProperties":
      case "cmd_InsertTable":
      case "cmd_InsertRowAbove":
      case "cmd_InsertRowBelow":
      case "cmd_InsertColumnBefore":
      case "cmd_InsertColumnAfter":
      case "cmd_SelectTable":
      case "cmd_SelectRow":
      case "cmd_SelectColumn":
      case "cmd_SelectCell":
      case "cmd_SelectAllCells":
      case "cmd_DeleteTable":
      case "cmd_DeleteRow":
      case "cmd_DeleteColumn":
      case "cmd_DeleteCell":
      case "cmd_DeleteCellContents":
      case "cmd_NormalizeTable":
      case "cmd_tableJoinCells":
      case "cmd_tableSplitCell":
      case "cmd_editTable":

      //Options Menu
      case "cmd_selectAddress":
      case "cmd_spelling":
      case "cmd_outputFormat":
      case "cmd_quoteMessage":
      case "cmd_rewrap":

        return true;

      default:
//        dump("##MsgCompose: command " + command + "no supported!\n");
        return false;
    }
  },
  isCommandEnabled: function(command)
  {
    //For some reason, when editor has the focus, focusedElement is null!.
    var focusedElement = top.document.commandDispatcher.focusedElement;

    var composeHTML = gMsgCompose && gMsgCompose.composeHTML;

    switch (command)
    {
      //File Menu
      case "cmd_attachFile":
      case "cmd_attachPage":
      case "cmd_close":
      case "cmd_saveDefault":
      case "cmd_saveAsFile":
      case "cmd_saveAsDraft":
      case "cmd_saveAsTemplate":
      case "cmd_sendButton":
      case "cmd_sendLater":
      case "cmd_printSetup":
      case "cmd_print":
      case "cmd_sendWithCheck":
        return !gWindowLocked;
      case "cmd_sendNow":
        return !(gWindowLocked || gIsOffline);
      case "cmd_quit":
        return true;

      //Edit Menu
      case "cmd_pasteQuote":
      case "cmd_find":
      case "cmd_findNext":
        //Disable the editor specific edit commands if the focus is not into the body
        return !focusedElement;
      case "cmd_delete":
        return MessageHasSelectedAttachments();
      case "cmd_selectAll":
        return MessageHasAttachments();
      case "cmd_account":
      case "cmd_preferences":
        return true;

      //View Menu
      case "cmd_showComposeToolbar":
        return true;
      case "cmd_showFormatToolbar":
        return composeHTML;

      //Insert Menu
      case "cmd_renderedHTMLEnabler":
      case "cmd_insert":
        return !focusedElement;
      case "cmd_link":
      case "cmd_anchor":
      case "cmd_image":
      case "cmd_hline":
      case "cmd_table":
      case "cmd_insertHTML":
      case "cmd_insertChars":
      case "cmd_insertBreak":
      case "cmd_insertBreakAll":
        return !focusedElement;

      //Options Menu
      case "cmd_selectAddress":
        return !gWindowLocked;
      case "cmd_spelling":
        return (focusedElement == GetMsgBodyFrame());
      case "cmd_outputFormat":
        return composeHTML;
      case "cmd_quoteMessage":
        try {
          gMailSession.topmostMsgWindow;
          return true;
        } catch (ex) { return false; }
      case "cmd_rewrap":
        return !composeHTML && !focusedElement;

      //Format Menu
      case "cmd_decreaseFont":
      case "cmd_increaseFont":
      case "cmd_bold":
      case "cmd_italic":
      case "cmd_underline":
      case "cmd_smiley":
      case "cmd_strikethrough":
      case "cmd_superscript":
      case "cmd_subscript":
      case "cmd_nobreak":
      case "cmd_em":
      case "cmd_strong":
      case "cmd_cite":
      case "cmd_abbr":
      case "cmd_acronym":
      case "cmd_code":
      case "cmd_samp":
      case "cmd_var":
      case "cmd_removeList":
      case "cmd_ul":
      case "cmd_ol":
      case "cmd_dt":
      case "cmd_dd":
      case "cmd_listProperties":
      case "cmd_indent":
      case "cmd_outdent":
      case "cmd_objectProperties":
      case "cmd_InsertTable":
      case "cmd_InsertRowAbove":
      case "cmd_InsertRowBelow":
      case "cmd_InsertColumnBefore":
      case "cmd_InsertColumnAfter":
      case "cmd_SelectTable":
      case "cmd_SelectRow":
      case "cmd_SelectColumn":
      case "cmd_SelectCell":
      case "cmd_SelectAllCells":
      case "cmd_DeleteTable":
      case "cmd_DeleteRow":
      case "cmd_DeleteColumn":
      case "cmd_DeleteCell":
      case "cmd_DeleteCellContents":
      case "cmd_NormalizeTable":
      case "cmd_tableJoinCells":
      case "cmd_tableSplitCell":
      case "cmd_editTable":
        return !focusedElement;

      default:
//        dump("##MsgCompose: command " + command + " disabled!\n");
        return false;
    }
  },

  doCommand: function(command)
  { 
    switch (command)
    {
      //File Menu
      case "cmd_attachFile"         : if (defaultController.isCommandEnabled(command)) AttachFile();           break;
      case "cmd_attachPage"         : AttachPage();           break;
      case "cmd_close"              : DoCommandClose();       break;
      case "cmd_saveDefault"        : Save();                 break;
      case "cmd_saveAsFile"         : SaveAsFile(true);       break;
      case "cmd_saveAsDraft"        : SaveAsDraft();          break;
      case "cmd_saveAsTemplate"     : SaveAsTemplate();       break;
      case "cmd_sendButton"         :
        if (defaultController.isCommandEnabled(command))
        {
          if (gIOService && gIOService.offline)
            SendMessageLater();
          else
            SendMessage();
        }
        break;
      case "cmd_sendNow"            : if (defaultController.isCommandEnabled(command)) SendMessage();          break;
      case "cmd_sendWithCheck"   : if (defaultController.isCommandEnabled(command)) SendMessageWithCheck();          break;
      case "cmd_sendLater"          : if (defaultController.isCommandEnabled(command)) SendMessageLater();     break;
      case "cmd_printSetup"         : NSPrintSetup();                                                           break;
      case "cmd_print"              : DoCommandPrint();                                                        break;

      //Edit Menu
      case "cmd_delete"             : if (MessageHasSelectedAttachments()) RemoveSelectedAttachment();         break;
      case "cmd_selectAll"          : if (MessageHasAttachments()) SelectAllAttachments();                     break;
      case "cmd_account"            : MsgAccountManager(null); break;
      case "cmd_preferences"        : DoCommandPreferences(); break;

      //View Menu
      case "cmd_showComposeToolbar" : goToggleToolbar('composeToolbar', 'menu_showComposeToolbar'); break;
      case "cmd_showFormatToolbar"  : goToggleToolbar('FormatToolbar', 'menu_showFormatToolbar');   break;

      //Options Menu
      case "cmd_selectAddress"      : if (defaultController.isCommandEnabled(command)) SelectAddress();         break;
      case "cmd_quoteMessage"       : if (defaultController.isCommandEnabled(command)) QuoteSelectedMessage();  break;
      case "cmd_rewrap"             :
          gMsgCompose.editor.QueryInterface(Components.interfaces.nsIEditorMailSupport);
          gMsgCompose.editor.rewrap(false);
          break;
      default:
//        dump("##MsgCompose: don't know what to do with command " + command + "!\n");
        return;
    }
  },

  onEvent: function(event)
  {
//    dump("DefaultController:onEvent\n");
  }
}

function QuoteSelectedMessage()
{
  if (gMsgCompose) {
    var mailWindow = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService()
                     .QueryInterface(Components.interfaces.nsIWindowMediator)
                     .getMostRecentWindow("mail:3pane");
    if (mailWindow) {
      var selectedURIs = mailWindow.GetSelectedMessages();
      if (selectedURIs)
        for (i = 0; i < selectedURIs.length; i++)
          gMsgCompose.quoteMessage(selectedURIs[i]);
    }
  }
}

function SetupCommandUpdateHandlers()
{
//  dump("SetupCommandUpdateHandlers\n");
  top.controllers.insertControllerAt(0, defaultController);
}

function CommandUpdate_MsgCompose()
{
  if (gSuppressCommandUpdating) {
    //dump("XXX supressing\n");
    return;
  }

  var element = top.document.commandDispatcher.focusedElement;

  // we're just setting focus to where it was before
  if (element == gLastElementToHaveFocus) {
    //dump("XXX skip\n");
    return;
  }

  gLastElementToHaveFocus = element;

  //dump("XXX update, focus on " + element + "\n");
  
  updateComposeItems();
}

function updateComposeItems() {
  try {
  //Edit Menu

  //Insert Menu
  if (gMsgCompose && gMsgCompose.composeHTML)
  {
    goUpdateCommand("cmd_insert");
    goUpdateCommand("cmd_decreaseFont");
    goUpdateCommand("cmd_increaseFont");
    goUpdateCommand("cmd_bold");
    goUpdateCommand("cmd_italic");
    goUpdateCommand("cmd_underline");
    goUpdateCommand("cmd_ul");
    goUpdateCommand("cmd_ol");
    goUpdateCommand("cmd_indent");
    goUpdateCommand("cmd_outdent");
    goUpdateCommand("cmd_align");
    goUpdateCommand("cmd_smiley");
  }

  //Options Menu
  goUpdateCommand("cmd_spelling");


  } catch(e) {}
}

function updateEditItems() {
  goUpdateCommand("cmd_pasteQuote");
  goUpdateCommand("cmd_delete");
  goUpdateCommand("cmd_selectAll");
  goUpdateCommand("cmd_find");
  goUpdateCommand("cmd_findNext");
}

var messageComposeOfflineObserver = {
  observe: function(subject, topic, state) {
    // sanity checks
    if (topic != "network:offline-status-changed") return;
    if (state == "offline")
      gIsOffline = true;
    else
      gIsOffline = false;
    MessageComposeOfflineStateChanged(gIsOffline);

    try {
        setupLdapAutocompleteSession();
    } catch (ex) {
        // catch the exception and ignore it, so that if LDAP setup 
        // fails, the entire compose window stuff doesn't get aborted
    }
  }
}

function AddMessageComposeOfflineObserver()
{
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.addObserver(messageComposeOfflineObserver, "network:offline-status-changed", false);
  
  gIsOffline = gIOService.offline;
  // set the initial state of the send button
  MessageComposeOfflineStateChanged(gIsOffline);
}

function RemoveMessageComposeOfflineObserver()
{
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.removeObserver(messageComposeOfflineObserver,"network:offline-status-changed");
}

function MessageComposeOfflineStateChanged(goingOffline)
{
  try {
    var sendButton = document.getElementById("button-send");
    var sendNowMenuItem = document.getElementById("menu-item-send-now");

    if (!gSavedSendNowKey) {
      gSavedSendNowKey = sendNowMenuItem.getAttribute('key');
    }

    // don't use goUpdateCommand here ... the defaultController might not be installed yet
    goSetCommandEnabled("cmd_sendNow", defaultController.isCommandEnabled("cmd_sendNow"));

    if (goingOffline)
    {
      sendButton.label = sendButton.getAttribute('later_label');
      sendButton.setAttribute('tooltiptext', sendButton.getAttribute('later_tooltiptext'));
      sendNowMenuItem.removeAttribute('key');
    }
    else
    {
      sendButton.label = sendButton.getAttribute('now_label');
      sendButton.setAttribute('tooltiptext', sendButton.getAttribute('now_tooltiptext'));
      if (gSavedSendNowKey) {
        sendNowMenuItem.setAttribute('key', gSavedSendNowKey);
      }
    }

  } catch(e) {}
}

var directoryServerObserver = {
  observe: function(subject, topic, value) {
      try {
          setupLdapAutocompleteSession();
      } catch (ex) {
          // catch the exception and ignore it, so that if LDAP setup 
          // fails, the entire compose window doesn't get horked
      }
  }
}

function AddDirectoryServerObserver(flag) {
  if (flag) {
    sPrefBranchInternal.addObserver("ldap_2.autoComplete.useDirectory",
                                    directoryServerObserver, false);
    sPrefBranchInternal.addObserver("ldap_2.autoComplete.directoryServer",
                                    directoryServerObserver, false);
  }
  else
  {
    var prefstring = "mail.identity." + gCurrentIdentity.key + ".overrideGlobal_Pref";
    sPrefBranchInternal.addObserver(prefstring, directoryServerObserver, false);
    prefstring = "mail.identity." + gCurrentIdentity.key + ".directoryServer";
    sPrefBranchInternal.addObserver(prefstring, directoryServerObserver, false);
  }
}

function RemoveDirectoryServerObserver(prefstring)
{
  if (!prefstring) {
    sPrefBranchInternal.removeObserver("ldap_2.autoComplete.useDirectory", directoryServerObserver);
    sPrefBranchInternal.removeObserver("ldap_2.autoComplete.directoryServer", directoryServerObserver);
  }
  else
  {
    var str = prefstring + ".overrideGlobal_Pref";
    sPrefBranchInternal.removeObserver(str, directoryServerObserver);
    str = prefstring + ".directoryServer";
    sPrefBranchInternal.removeObserver(str, directoryServerObserver);
  }
}

function AddDirectorySettingsObserver()
{
  sPrefBranchInternal.addObserver(gCurrentAutocompleteDirectory, directoryServerObserver, false);
}

function RemoveDirectorySettingsObserver(prefstring)
{
  sPrefBranchInternal.removeObserver(prefstring, directoryServerObserver);
}

function setupLdapAutocompleteSession()
{
    var autocompleteLdap = false;
    var autocompleteDirectory = null;
    var prevAutocompleteDirectory = gCurrentAutocompleteDirectory;
    var i;

    autocompleteLdap = sPrefs.getBoolPref("ldap_2.autoComplete.useDirectory");
    if (autocompleteLdap)
        autocompleteDirectory = sPrefs.getCharPref(
            "ldap_2.autoComplete.directoryServer");

    if(gCurrentIdentity.overrideGlobalPref) {
        autocompleteDirectory = gCurrentIdentity.directoryServer;
    }

    // use a temporary to do the setup so that we don't overwrite the
    // global, then have some problem and throw an exception, and leave the
    // global with a partially setup session.  we'll assign the temp
    // into the global after we're done setting up the session
    //
    var LDAPSession;
    if (gLDAPSession) {
        LDAPSession = gLDAPSession;
    } else {
        LDAPSession = Components.classes[
            "@mozilla.org/autocompleteSession;1?type=ldap"].createInstance()
            .QueryInterface(Components.interfaces.nsILDAPAutoCompleteSession);
    }
            
    if (autocompleteDirectory && !gIsOffline) { 
        // Add observer on the directory server we are autocompleting against
        // only if current server is different from previous.
        // Remove observer if current server is different from previous       
        gCurrentAutocompleteDirectory = autocompleteDirectory;
        if (prevAutocompleteDirectory) {
          if (prevAutocompleteDirectory != gCurrentAutocompleteDirectory) { 
            RemoveDirectorySettingsObserver(prevAutocompleteDirectory);
            AddDirectorySettingsObserver();
          }
        }
        else
          AddDirectorySettingsObserver();
        
        // fill in the session params if there is a session
        //
        if (LDAPSession) {
            var serverURL = Components.classes[
                "@mozilla.org/network/ldap-url;1"].
                createInstance().QueryInterface(
                    Components.interfaces.nsILDAPURL);

            try {
                serverURL.spec = sPrefs.getComplexValue(autocompleteDirectory +".uri",
                                           Components.interfaces.nsISupportsString).data;
            } catch (ex) {
                dump("ERROR: " + ex + "\n");
            }
            LDAPSession.serverURL = serverURL;

            // get the login to authenticate as, if there is one
            //
            var login = "";
            try {
                login = sPrefs.getComplexValue(
                    autocompleteDirectory + ".auth.dn",
                    Components.interfaces.nsISupportsString).data;
            } catch (ex) {
                // if we don't have this pref, no big deal
            }

            // find out if we need to authenticate, and if so, tell the LDAP
            // autocomplete session how to prompt for a password.  This window
            // (the compose window) is being used to parent the authprompter.
            //
            LDAPSession.login = login;
            if (login != "") {
                var windowWatcherSvc = Components.classes[
                    "@mozilla.org/embedcomp/window-watcher;1"]
                    .getService(Components.interfaces.nsIWindowWatcher);
                var domWin = 
                    window.QueryInterface(Components.interfaces.nsIDOMWindow);
                var authPrompter = 
                    windowWatcherSvc.getNewAuthPrompter(domWin);

                LDAPSession.authPrompter = authPrompter;
            }

            // don't search on non-CJK strings shorter than this
            //
            try { 
                LDAPSession.minStringLength = sPrefs.getIntPref(
                    autocompleteDirectory + ".autoComplete.minStringLength");
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default.
            }

            // don't search on CJK strings shorter than this
            //
            try { 
                LDAPSession.cjkMinStringLength = sPrefs.getIntPref(
                  autocompleteDirectory + ".autoComplete.cjkMinStringLength");
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default.
            }

            // we don't try/catch here, because if this fails, we're outta luck
            //
            var ldapFormatter = Components.classes[
                "@mozilla.org/ldap-autocomplete-formatter;1?type=addrbook"]
                .createInstance().QueryInterface(
                    Components.interfaces.nsIAbLDAPAutoCompFormatter);

            // override autocomplete name format?
            //
            try {
                ldapFormatter.nameFormat = 
                    sPrefs.getComplexValue(autocompleteDirectory + 
                                      ".autoComplete.nameFormat",
                                      Components.interfaces.nsISupportsString).data;
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsAbLDAPAutoCompFormatter use its default.
            }

            // override autocomplete mail address format?
            //
            try {
                ldapFormatter.addressFormat = 
                    sPrefs.getComplexValue(autocompleteDirectory + 
                                      ".autoComplete.addressFormat",
                                      Components.interfaces.nsISupportsString).data;
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsAbLDAPAutoCompFormatter use its default.
            }

            try {
                // figure out what goes in the comment column, if anything
                //
                // 0 = none
                // 1 = name of addressbook this card came from
                // 2 = other per-addressbook format
                //
                var showComments = 0;
                showComments = sPrefs.getIntPref(
                    "mail.autoComplete.commentColumn");

                switch (showComments) {

                case 1:
                    // use the name of this directory
                    //
                    ldapFormatter.commentFormat = sPrefs.getComplexValue(
                                autocompleteDirectory + ".description",
                                Components.interfaces.nsISupportsString).data;
                    break;

                case 2:
                    // override ldap-specific autocomplete entry?
                    //
                    try {
                        ldapFormatter.commentFormat = 
                            sPrefs.getComplexValue(autocompleteDirectory + 
                                        ".autoComplete.commentFormat",
                                        Components.interfaces.nsISupportsString).data;
                    } catch (innerException) {
                        // if nothing has been specified, use the ldap
                        // organization field
                        ldapFormatter.commentFormat = "[o]";
                    }
                    break;

                case 0:
                default:
                    // do nothing
                }
            } catch (ex) {
                // if something went wrong while setting up comments, try and
                // proceed anyway
            }

            // set the session's formatter, which also happens to
            // force a call to the formatter's getAttributes() method
            // -- which is why this needs to happen after we've set the
            // various formats
            //
            LDAPSession.formatter = ldapFormatter;

            // override autocomplete entry formatting?
            //
            try {
                LDAPSession.outputFormat = 
                    sPrefs.getComplexValue(autocompleteDirectory + 
                                      ".autoComplete.outputFormat",
                                      Components.interfaces.nsISupportsString).data;

            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default.
            }

            // override default search filter template?
            //
            try { 
                LDAPSession.filterTemplate = sPrefs.getComplexValue(
                    autocompleteDirectory + ".autoComplete.filterTemplate",
                    Components.interfaces.nsISupportsString).data;

            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default
            }

            // override default maxHits (currently 100)
            //
            try { 
                // XXXdmose should really use .autocomplete.maxHits,
                // but there's no UI for that yet
                // 
                LDAPSession.maxHits = 
                    sPrefs.getIntPref(autocompleteDirectory + ".maxHits");
            } catch (ex) {
                // if this pref isn't there, or is out of range, no big deal. 
                // just let nsLDAPAutoCompleteSession use its default.
            }

            if (!gSessionAdded) {
                // if we make it here, we know that session initialization has
                // succeeded; add the session for all recipients, and 
                // remember that we've done so
                var autoCompleteWidget;
                for (i=1; i <= awGetMaxRecipients(); i++)
                {
                    autoCompleteWidget = document.getElementById("addressCol2#" + i);
                    if (autoCompleteWidget)
                    {
                      autoCompleteWidget.addSession(LDAPSession);
                      // ldap searches don't insert a default entry with the default domain appended to it
                      // so reduce the minimum results for a popup to 2 in this case. 
                      autoCompleteWidget.minResultsForPopup = 2;

                    }
                 }
                gSessionAdded = true;
            }
        }
    } else {
      if (gCurrentAutocompleteDirectory) {
        // Remove observer on the directory server since we are not doing Ldap
        // autocompletion.
        RemoveDirectorySettingsObserver(gCurrentAutocompleteDirectory);
        gCurrentAutocompleteDirectory = null;
      }
      if (gLDAPSession && gSessionAdded) {
        for (i=1; i <= awGetMaxRecipients(); i++) 
          document.getElementById("addressCol2#" + i).
              removeSession(gLDAPSession);
        gSessionAdded = false;
      }
    }

    gLDAPSession = LDAPSession;
    gSetupLdapAutocomplete = true;
}

function DoCommandClose()
{
  var retVal;
  if ((retVal = ComposeCanClose())) {

    // Notify the SendListener that Send has been aborted and Stopped
    if (gMsgCompose)
    {
      var externalListener = gMsgCompose.getExternalSendListener();
      if (externalListener)
      {
        externalListener.onSendNotPerformed(null, Components.results.NS_ERROR_ABORT);
      }
    }

    MsgComposeCloseWindow(true);
	  // at this point, we might be caching this window.
	  // in which case, we don't want to close it
    if (sMsgComposeService.isCachedWindow(window)) {
	    retVal = false;
	  }
  }

  return retVal;
}

function DoCommandPrint()
{
  if (gMsgCompose)
  {
    var editorShell = gMsgCompose.editor;
    if (editorShell)
      try {
        editorShell.FinishHTMLSource();
        editorShell.Print();
      } catch(ex) {dump("#PRINT ERROR: " + ex + "\n");}
  }
}

function DoCommandPreferences()
{
  goPreferences('mailnews', 'chrome://messenger/content/messengercompose/pref-composing_messages.xul', 'mailcomposepref');
}

function ToggleWindowLock()
{
  gWindowLocked = !gWindowLocked;
  CommandUpdate_MsgCompose();
}

/* This function will go away soon as now arguments are passed to the window using a object of type nsMsgComposeParams instead of a string */
function GetArgs(originalData)
{
  var args = new Object();

  if (originalData == "")
    return null;

  var data = "";
  var separator = String.fromCharCode(1);

  var quoteChar = "";
  var prevChar = "";
  var nextChar = "";
  for (var i = 0; i < originalData.length; i ++, prevChar = aChar)
  {
    var aChar = originalData.charAt(i)
    var aCharCode = originalData.charCodeAt(i)
    if ( i < originalData.length - 1)
      nextChar = originalData.charAt(i + 1);
    else
      nextChar = "";

    if (aChar == quoteChar && (nextChar == "," || nextChar == ""))
    {
      quoteChar = "";
      data += aChar;
    }
    else if ((aCharCode == 39 || aCharCode == 34) && prevChar == "=") //quote or double quote
    {
      if (quoteChar == "")
        quoteChar = aChar;
      data += aChar;
    }
    else if (aChar == ",")
    {
      if (quoteChar == "")
        data += separator;
      else
        data += aChar
    }
    else
      data += aChar
  }

  var pairs = data.split(separator);
//  dump("Compose: argument: {" + data + "}\n");

  for (i = pairs.length - 1; i >= 0; i--)
  {
    var pos = pairs[i].indexOf('=');
    if (pos == -1)
      continue;
    var argname = pairs[i].substring(0, pos);
    var argvalue = pairs[i].substring(pos + 1);
    if (argvalue.charAt(0) == "'" && argvalue.charAt(argvalue.length - 1) == "'")
      args[argname] = argvalue.substring(1, argvalue.length - 1);
    else
      try {
        args[argname] = unescape(argvalue);
      } catch (e) {args[argname] = argvalue;}
    dump("[" + argname + "=" + args[argname] + "]\n");
  }
  return args;
}

function ComposeFieldsReady(msgType)
{
  //If we are in plain text, we need to set the wrap column
  if (! gMsgCompose.composeHTML) {
    try {
      window.editorShell.wrapColumn = gMsgCompose.wrapLength;
    }
    catch (e) {
      dump("### window.editorShell.wrapColumn exception text: " + e + " - failed\n");
    }
  }
  CompFields2Recipients(gMsgCompose.compFields, gMsgCompose.type);
  SetComposeWindowTitle();
  AdjustFocus();
}

function ComposeStartup(recycled, aParams)
{
  var params = null; // New way to pass parameters to the compose window as a nsIMsgComposeParameters object
  var args = null;   // old way, parameters are passed as a string
  
  if (recycled)
    dump("This is a recycled compose window!\n");

  if (aParams)
    params = aParams;
  else
    if (window.arguments && window.arguments[0]) {
      try {
        params = window.arguments[0].QueryInterface(Components.interfaces.nsIMsgComposeParams);
      }
      catch(ex) { dump("ERROR with parameters: " + ex + "\n"); }
      if (!params)
        args = GetArgs(window.arguments[0]);
    }

  var identityList = document.getElementById("msgIdentity");
  var identityListPopup = document.getElementById("msgIdentityPopup");

  document.addEventListener("keypress", awDocumentKeyPress, true);

  if (identityListPopup)
    FillIdentityListPopup(identityListPopup);

  if (!params) {
    // This code will go away soon as now arguments are passed to the window using a object of type nsMsgComposeParams instead of a string

    params = Components.classes["@mozilla.org/messengercompose/composeparams;1"].createInstance(Components.interfaces.nsIMsgComposeParams);
    params.composeFields = Components.classes["@mozilla.org/messengercompose/composefields;1"].createInstance(Components.interfaces.nsIMsgCompFields);

    if (args) { //Convert old fashion arguments into params
      var composeFields = params.composeFields;
      if (args.bodyislink == "true")
        params.bodyIsLink = true;
      if (args.type)
        params.type = args.type;
      if (args.format)
        params.format = args.format;
      if (args.originalMsg)
        params.originalMsgURI = args.originalMsg;
      if (args.preselectid)
        params.identity = getIdentityForKey(args.preselectid);
      if (args.to)
        composeFields.to = args.to;
      if (args.cc)
        composeFields.cc = args.cc;
      if (args.bcc)
        composeFields.bcc = args.bcc;
      if (args.newsgroups)
        composeFields.newsgroups = args.newsgroups;
      if (args.subject)
        composeFields.subject = args.subject;
      if (args.attachment)
      {
        var attachmentList = args.attachment.split(",");
        var attachment;
        for (var i = 0; i < attachmentList.length; i ++)
        {
          attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].createInstance(Components.interfaces.nsIMsgAttachment);
          attachment.url = attachmentList[i];
          composeFields.addAttachment(attachment);
        }
      }
      if (args.newshost)
        composeFields.newshost = args.newshost;
      if (args.body)
         composeFields.body = args.body;
    }
  }

  // when editor has focus, top.document.commandDispatcher.focusedElement is null,
  // it's also null during a blur.
  // if we are doing a new message, originalMsgURI is null, so
  // we'll default gLastElementToHaveFocus to null, to skip blurs, since we're
  // going to be setting focus to the addressing widget.
  //
  // for reply or fwd, originalMsgURI is non-null, so we'll
  // default gLastElementToHaveFocus to 1, so that when focus gets set on editor
  // we'll do an update.
  if (params.originalMsgURI) {
    gLastElementToHaveFocus = 1;
  }
  else {
    gLastElementToHaveFocus = null;
  }

  if (!params.identity) {
    // no pre selected identity, so use the default account
    var identities = gAccountManager.defaultAccount.identities;
    if (identities.Count() >= 1)
      params.identity = identities.QueryElementAt(0, Components.interfaces.nsIMsgIdentity);
    else
    {
      identities = GetIdentities();
      params.identity = identities[0];
    }
  }

  for (i = 0; i < identityListPopup.childNodes.length;i++) {
    var item = identityListPopup.childNodes[i];
    var id = item.getAttribute('id');
    if (id == params.identity.key) {
        identityList.selectedItem = item;
        break;
    }
  }
  LoadIdentity(true);
  if (sMsgComposeService)
  {
    gMsgCompose = sMsgComposeService.InitCompose(window, params);
    if (gMsgCompose)
    {
      // set the close listener
      gMsgCompose.recyclingListener = gComposeRecyclingListener;
      
      //Lets the compose object knows that we are dealing with a recycled window
      gMsgCompose.recycledWindow = recycled;

      //Creating a Editor Shell
      var editorElement = document.getElementById("content-frame");
      if (!editorElement)
      {
        dump("Failed to get editor element!\n");
        return;
      }
      var editorShell = editorElement.editorShell;
      if (!editorShell)
      {
        dump("Failed to create editorShell!\n");
        return;
      }

      document.getElementById("returnReceiptMenu").setAttribute('checked', 
                                         gMsgCompose.compFields.returnReceipt);

      if (!recycled) //The editor is already initialized and does not support to be re-initialized.
      {
        // save the editorShell in the window. The editor JS expects to find it there.
        window.editorShell = editorShell;

        // setEditorType MUST be call before setContentWindow
        if (gMsgCompose.composeHTML)
          window.editorShell.editorType = "htmlmail";
        else
        {
          //Remove HTML toolbar, format and insert menus as we are editing in plain text mode
          document.getElementById("outputFormatMenu").setAttribute("hidden", true);
          document.getElementById("FormatToolbar").setAttribute("hidden", true);
          document.getElementById("formatMenu").setAttribute("hidden", true);
          document.getElementById("insertMenu").setAttribute("hidden", true);
          document.getElementById("menu_showFormatToolbar").setAttribute("hidden", true);

          window.editorShell.editorType = "textmail";
        }
        window.editorShell.webShellWindow = window;
        window.editorShell.contentWindow = window._content;

        // Do setup common to Message Composer and Web Composer
        EditorSharedStartup();   
   
      }

      var msgCompFields = gMsgCompose.compFields;
      if (msgCompFields)
      {
        if (params.bodyIsLink)
        {
          var body = msgCompFields.body;
          if (gMsgCompose.composeHTML)
          {
            var cleanBody;
            try {
              cleanBody = unescape(body);
            } catch(e) { cleanBody = body;}

            msgCompFields.body = "<BR><A HREF=\"" + body + "\">" + cleanBody + "</A><BR>";
          }
          else
            msgCompFields.body = "\n<" + body + ">\n";
        }

        var subjectValue = msgCompFields.subject;
        document.getElementById("msgSubject").value = subjectValue;

        var attachments = msgCompFields.attachmentsArray;
        if (attachments)
          for (i = 0; i < attachments.Count(); i ++)
            AddAttachment(attachments.QueryElementAt(i, Components.interfaces.nsIMsgAttachment));
        }

      gMsgCompose.RegisterStateListener(stateListener);
      gMsgCompose.editor = window.editorShell;      
    }
  }
}

function WizCallback(state)
{
  if (state){
    ComposeStartup(false, null);
  }
  else
  {
    if (gMsgCompose)
      gMsgCompose.CloseWindow(false); //Don't try to recyle a bogus window
    else
      window.close();
//  window.tryToClose=ComposeCanClose;
  }
}

function ComposeLoad()
{
  // First get the preferences service
  try {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
    sPrefs = prefService.getBranch(null);
    sPrefBranchInternal = sPrefs.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
  }
  catch (ex) {
    dump("failed to preferences services\n");
  }

  try {
    sOther_headers = sPrefs.getCharPref("mail.compose.other.header");
  }
  catch (ex) {
    dump("failed to get the mail.compose.other.header pref\n");
  }

  AddMessageComposeOfflineObserver();
  AddDirectoryServerObserver(true);

  if (gLogComposePerformance)
    sMsgComposeService.TimeStamp("Start initializing the compose window (ComposeLoad)", false);

  try {
    SetupCommandUpdateHandlers();
    var wizardcallback = true;
    var state = verifyAccounts(wizardcallback); // this will do migration, or create a new account if we need to.

    if (sOther_headers) {
      var selectNode = document.getElementById('addressCol1#1');
      var sOther_headers_Array = sOther_headers.split(",");
      for (var i = 0; i < sOther_headers_Array.length; i++)
        selectNode.appendItem(sOther_headers_Array[i] + ":", "addr_other");
    }
    if (state)
      ComposeStartup(false, null);
  }
  catch (ex) {
    var errorTitle = sComposeMsgsBundle.getString("initErrorDlogTitle");
    var errorMsg = sComposeMsgsBundle.getFormattedString("initErrorDlogMessage",
                                                         [ex]);
    if (gPromptService)
      gPromptService.alert(window, errorTitle, errorMsg);
    else
      window.alert(errorMsg);

    if (gMsgCompose)
      gMsgCompose.CloseWindow(false); //Don't try to recycle a bogus window
    else
      window.close();
    return;
  }
  window.tryToClose=ComposeCanClose;
  if (gLogComposePerformance)
    sMsgComposeService.TimeStamp("Done with the initialization (ComposeLoad). Waiting on editor to load about:blank", false);
}

function ComposeUnload()
{
  dump("\nComposeUnload from XUL\n");

  EditorCleanup();

  RemoveMessageComposeOfflineObserver();
  RemoveDirectoryServerObserver(null);
  if (gCurrentIdentity)
    RemoveDirectoryServerObserver("mail.identity." + gCurrentIdentity.key);
  if (gCurrentAutocompleteDirectory)
    RemoveDirectorySettingsObserver(gCurrentAutocompleteDirectory);
  if (gMsgCompose)
    gMsgCompose.UnregisterStateListener(stateListener);
}

function SetDocumentCharacterSet(aCharset)
{
  dump("SetDocumentCharacterSet Callback!\n");
  dump(aCharset + "\n");

  if (gMsgCompose) {
    gMsgCompose.SetDocumentCharset(aCharset);
    gCurrentMailSendCharset = aCharset;
    gCharsetTitle = null;
    SetComposeWindowTitle();
  }
  else
    dump("Compose has not been created!\n");
}

function UpdateMailEditCharset()
{
  var send_default_charset = gMsgCompose.compFields.defaultCharacterSet;
//  dump("send_default_charset is " + send_default_charset + "\n");

  var compFieldsCharset = gMsgCompose.compFields.characterSet;
//  dump("gMsgCompose.compFields is " + compFieldsCharset + "\n");

  if (gCharsetConvertManager) {
    var charsetAtom = gCharsetConvertManager.GetCharsetAtom(compFieldsCharset);
    if (charsetAtom && (charsetAtom.GetUnicode() == "us-ascii"))
      compFieldsCharset = "ISO-8859-1";   // no menu item for "us-ascii"
  }

  // charset may have been set implicitly in case of reply/forward
  // or use pref default otherwise
  var menuitem = document.getElementById(send_default_charset == compFieldsCharset ? 
                                         send_default_charset : compFieldsCharset);
  if (menuitem)
    menuitem.setAttribute('checked', 'true');

  // Set a document charset to a default mail send charset.
  if (send_default_charset == compFieldsCharset)
    SetDocumentCharacterSet(send_default_charset);
}

function InitCharsetMenuCheckMark()
{
  // return if the charset is already set explitily
  if (gCurrentMailSendCharset != null) {
    dump("already set to " + gCurrentMailSendCharset + "\n");
    return;
  }

  // Check the menu
  UpdateMailEditCharset();
  // use setTimeout workaround to delay checkmark the menu
  // when onmenucomplete is ready then use it instead of oncreate
  // see bug 78290 for the detail
  setTimeout("UpdateMailEditCharset()", 0);

}

function GetCharsetUIString()
{
  var charset = gMsgCompose.compFields.characterSet;
  if (gSendDefaultCharset == null) {
    gSendDefaultCharset = gMsgCompose.compFields.defaultCharacterSet;
  }

  charset = charset.toUpperCase();
  if (charset == "US-ASCII")
    charset = "ISO-8859-1";

  if (charset != gSendDefaultCharset) {

    if (gCharsetTitle == null) {
      try {
        // check if we have a converter for this charset
        var charsetAtom = gCharsetConvertManager.GetCharsetAtom(charset);
        var encoderList = gCharsetConvertManager.GetEncoderList();
        var n = encoderList.Count();
        var found = false;
        for (var i = 0; i < n; i++)
        {
          if (charsetAtom == encoderList.GetElementAt(i))
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
          dump("no charset converter available for " +  charset + " default charset is used instead\n");
          // set to default charset, no need to show it in the window title
          gMsgCompose.compFields.characterSet = gSendDefaultCharset;
          return "";
        }

        // get a localized string
        gCharsetTitle = gCharsetConvertManager.GetCharsetTitle(charsetAtom);
      }
      catch (ex) {
        dump("failed to get a charset title of " + charset + "!\n");
        gCharsetTitle = charset; // just show the charset itself
      }
    }

    return " - " + gCharsetTitle;
  }

  return "";
}

function GenericSendMessage( msgType )
{
  dump("GenericSendMessage from XUL\n");

  dump("Identity = " + getCurrentIdentity() + "\n");

  if (gMsgCompose != null)
  {
      var msgCompFields = gMsgCompose.compFields;
      if (msgCompFields)
      {
      Recipients2CompFields(msgCompFields);
      var subject = document.getElementById("msgSubject").value;
      msgCompFields.subject = subject;
      Attachments2CompFields(msgCompFields);

      var event = document.createEvent('Events');
      event.initEvent('compose-send-message', false, true);
      document.getElementById("msgcomposeWindow").dispatchEvent(event);

      if (msgType == nsIMsgCompDeliverMode.Now || msgType == nsIMsgCompDeliverMode.Later)
      {
        //Do we need to check the spelling?
        if (sPrefs.getBoolPref("mail.SpellCheckBeforeSend")){
        //We disable spellcheck for the following -subject line, attachment pane, identity and addressing widget
        //therefore we need to explicitly focus on the mail body when we have to do a spellcheck.
          editorShell.contentWindow.focus();
          window.cancelSendMessage = false;
          try {
            window.openDialog("chrome://editor/content/EdSpellCheck.xul", "_blank",
                    "chrome,close,titlebar,modal", true);
          }
          catch(ex){}
          if(window.cancelSendMessage)
            return;
        }

        //Check if we have a subject, else ask user for confirmation
        if (subject == "")
        {
          if (gPromptService)
          {
            var result = {value:sComposeMsgsBundle.getString("defaultSubject")};
            if (gPromptService.prompt(
              window,
              sComposeMsgsBundle.getString("subjectDlogTitle"),
              sComposeMsgsBundle.getString("subjectDlogMessage"),
                        result,
              null,
              {value:0}
              ))
              {
                msgCompFields.subject = result.value;
                var subjectInputElem = document.getElementById("msgSubject");
                subjectInputElem.value = result.value;
              }
              else
                return;
            }
          }

        // Before sending the message, check what to do with HTML message, eventually abort.
        var convert = DetermineConvertibility();
        var action = DetermineHTMLAction(convert);
        if (action == nsIMsgCompSendFormat.AskUser)
        {
                    var recommAction = convert == nsIMsgCompConvertible.No
                                   ? nsIMsgCompSendFormat.AskUser
                                   : nsIMsgCompSendFormat.PlainText;
                    var result2 = {action:recommAction,
                                  convertible:convert,
                                  abort:false};
                    window.openDialog("chrome://messenger/content/messengercompose/askSendFormat.xul",
                                      "askSendFormatDialog", "chrome,modal,titlebar,centerscreen",
                                      result2);
          if (result2.abort)
            return;
          action = result2.action;
        }
        switch (action)
        {
          case nsIMsgCompSendFormat.PlainText:
            msgCompFields.forcePlainText = true;
            msgCompFields.useMultipartAlternative = false;
            break;
          case nsIMsgCompSendFormat.HTML:
            msgCompFields.forcePlainText = false;
            msgCompFields.useMultipartAlternative = false;
            break;
          case nsIMsgCompSendFormat.Both:
            msgCompFields.forcePlainText = false;
            msgCompFields.useMultipartAlternative = true;
            break;
           default: dump("\###SendMessage Error: invalid action value\n"); return;
        }
      }

      // hook for extra compose pre-processing
      var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
      observerService.notifyObservers(window, "mail:composeOnSend", null);

      // Check if the headers of composing mail can be converted to a mail charset.
      if (msgType == nsIMsgCompDeliverMode.Now || 
        msgType == nsIMsgCompDeliverMode.Later ||
        msgType == nsIMsgCompDeliverMode.Save || 
        msgType == nsIMsgCompDeliverMode.SaveAsDraft || 
        msgType == nsIMsgCompDeliverMode.SaveAsTemplate) 
      {
        var fallbackCharset = new Object;
        if (gPromptService && 
            !gMsgCompose.checkCharsetConversion(getCurrentIdentity(), fallbackCharset)) 
        {
          var dlgTitle = sComposeMsgsBundle.getString("initErrorDlogTitle");
          var dlgText = sComposeMsgsBundle.getString("12553");  // NS_ERROR_MSG_MULTILINGUAL_SEND
          if (!gPromptService.confirm(window, dlgTitle, dlgText))
            return;
        }
        if (fallbackCharset && 
            fallbackCharset.value && fallbackCharset.value != "")
          gMsgCompose.SetDocumentCharset(fallbackCharset.value);
      }
      try {
        gWindowLocked = true;
        CommandUpdate_MsgCompose();
        disableEditableFields();

        var progress = Components.classes["@mozilla.org/messenger/progress;1"].createInstance(Components.interfaces.nsIMsgProgress);
        if (progress)
        {
          progress.registerListener(progressListener);
          gSendOrSaveOperationInProgress = true;
        }
        gMsgCompose.SendMsg(msgType, getCurrentIdentity(), progress);
      }
      catch (ex) {
        dump("failed to SendMsg: " + ex + "\n");
        gWindowLocked = false;
        enableEditableFields();
        CommandUpdate_MsgCompose();
      }
    }
  }
  else
    dump("###SendMessage Error: composeAppCore is null!\n");
}

function SendMessage()
{
  dump("SendMessage from XUL\n");
  GenericSendMessage(nsIMsgCompDeliverMode.Now);
}

function SendMessageWithCheck()
{
    var warn = sPrefs.getBoolPref("mail.warn_on_send_accel_key");

    if (warn) {
        var checkValue = {value:false};
        var buttonPressed = gPromptService.confirmEx(window, 
              sComposeMsgsBundle.getString('sendMessageCheckWindowTitle'), 
              sComposeMsgsBundle.getString('sendMessageCheckLabel'),
              (gPromptService.BUTTON_TITLE_IS_STRING * gPromptService.BUTTON_POS_0) +
              (gPromptService.BUTTON_TITLE_CANCEL * gPromptService.BUTTON_POS_1),
              sComposeMsgsBundle.getString('sendMessageCheckSendButtonLabel'),
              null, null,
              sComposeMsgsBundle.getString('CheckMsg'), 
              checkValue);
        if (buttonPressed != 0) {
            return;
        }
        if (checkValue.value) {
            sPrefs.setBoolPref("mail.warn_on_send_accel_key", false);
        }
    }

  GenericSendMessage(gIsOffline ? nsIMsgCompDeliverMode.Later
                                 : nsIMsgCompDeliverMode.Now);
}

function SendMessageLater()
{
  dump("SendMessageLater from XUL\n");
  GenericSendMessage(nsIMsgCompDeliverMode.Later);
}

function Save()
{
  dump("Save from XUL\n");
  switch (defaultSaveOperation)
  {
    case "file"     : SaveAsFile(false);      break;
    case "template" : SaveAsTemplate(false);  break;
    default         : SaveAsDraft(false);     break;
  }
}

function SaveAsFile(saveAs)
{
  dump("SaveAsFile from XUL\n");
  if (gMsgCompose.bodyConvertible() == nsIMsgCompConvertible.Plain)
    SaveDocument(saveAs, false, "text/plain");
  else
    SaveDocument(saveAs, false, "text/html");
  defaultSaveOperation = "file";
}

function SaveAsDraft()
{
  dump("SaveAsDraft from XUL\n");

  GenericSendMessage(nsIMsgCompDeliverMode.SaveAsDraft);
  defaultSaveOperation = "draft";
}

function SaveAsTemplate()
{
  dump("SaveAsTemplate from XUL\n");

  GenericSendMessage(nsIMsgCompDeliverMode.SaveAsTemplate);
  defaultSaveOperation = "template";
}


function MessageFcc(menuItem)
{
  // Get the id for the folder we're FCC into
  // This is the additional FCC in addition to the
  // default FCC
  destUri = menuItem.getAttribute('id');
  if (gMsgCompose)
  {
    var msgCompFields = gMsgCompose.compFields;
    if (msgCompFields)
    {
      if (msgCompFields.fcc2 == destUri)
      {
        msgCompFields.fcc2 = "nocopy://";
        dump("FCC2: none\n");
      }
      else
      {
        msgCompFields.fcc2 = destUri;
        dump("FCC2: " + destUri + "\n");
      }
    }
  }
}

function PriorityMenuSelect(target)
{
  if (gMsgCompose)
  {
    var msgCompFields = gMsgCompose.compFields;
    if (msgCompFields)
      switch (target.getAttribute('id'))
      {
        case "priority_lowest":  msgCompFields.priority = "lowest";   break;
        case "priority_low":     msgCompFields.priority = "low";      break;
        case "priority_normal":  msgCompFields.priority = "normal";   break;
        case "priority_high":    msgCompFields.priority = "high";     break;
        case "priotity_highest": msgCompFields.priority = "highest";  break;
      }
  }
}

function OutputFormatMenuSelect(target)
{
  if (gMsgCompose)
  {
    var msgCompFields = gMsgCompose.compFields;
    var toolbar = document.getElementById("FormatToolbar");
    var format_menubar = document.getElementById("formatMenu");
    var insert_menubar = document.getElementById("insertMenu");
    var show_menuitem = document.getElementById("menu_showFormatToolbar");

    if (msgCompFields)
      switch (target.getAttribute('id'))
      {
        case "format_auto":  gSendFormat = nsIMsgCompSendFormat.AskUser;     break;
        case "format_plain": gSendFormat = nsIMsgCompSendFormat.PlainText;   break;
        case "format_html":  gSendFormat = nsIMsgCompSendFormat.HTML;        break;
        case "format_both":  gSendFormat = nsIMsgCompSendFormat.Both;        break;
      }
    gHideMenus = (gSendFormat == nsIMsgCompSendFormat.PlainText);
    format_menubar.hidden = gHideMenus;
    insert_menubar.hidden = gHideMenus;
    show_menuitem.hidden = gHideMenus;
    toolbar.hidden = gHideMenus ||
      (show_menuitem.getAttribute("checked") == "false");
  }
}

function SelectAddress()
{
  var msgCompFields = gMsgCompose.compFields;

  Recipients2CompFields(msgCompFields);

  var toAddress = msgCompFields.to;
  var ccAddress = msgCompFields.cc;
  var bccAddress = msgCompFields.bcc;

  dump("toAddress: " + toAddress + "\n");
  window.openDialog("chrome://messenger/content/addressbook/abSelectAddressesDialog.xul",
            "",
            "chrome,resizable,titlebar,modal",
            {composeWindow:top.window,
             msgCompFields:msgCompFields,
             toAddress:toAddress,
             ccAddress:ccAddress,
             bccAddress:bccAddress});
  // We have to set focus to the addressingwidget because we seem to loose focus often 
  // after opening the SelectAddresses Dialog- bug # 89950
  AdjustFocus();
}

function ToggleReturnReceipt(target)
{
    var msgCompFields = gMsgCompose.compFields;
    if (msgCompFields)
    {
        msgCompFields.returnReceipt = ! msgCompFields.returnReceipt;
        target.setAttribute('checked', msgCompFields.returnReceipt);
        gReceiptOptionChanged = true;
    }
}

function queryISupportsArray(supportsArray, iid) {
    var result = new Array;
    for (var i=0; i<supportsArray.Count(); i++) {
      // dump(i + "," + result[i] + "\n");
      result[i] = supportsArray.GetElementAt(i).QueryInterface(iid);
    }
    return result;
}

function GetIdentities()
{
    var idSupports = gAccountManager.allIdentities;
    var identities = queryISupportsArray(idSupports,
                                         Components.interfaces.nsIMsgIdentity);

    dump(identities + "\n");
    return identities;
}

function ClearIdentityListPopup(popup)
{
  if (popup)
    for (var i = popup.childNodes.length - 1; i >= 0; i--)
      popup.removeChild(popup.childNodes[i]);
}

function FillIdentityListPopup(popup)
{
    var identities = GetIdentities();

    for (var i=0; i<identities.length; i++)
    {
    var identity = identities[i];

    //dump(i + " = " + identity.identityName + "," +identity.key + "\n");

    //Get server prettyName for each identity

    var serverSupports = gAccountManager.GetServersForIdentity(identity);

    //dump(i + " = " + identity.identityName + "," +identity.key + "\n");

    if(serverSupports.GetElementAt(0))
      var result = serverSupports.GetElementAt(0).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
    //dump ("The account name is = "+result.prettyName+ "\n");
    var accountName = " - "+result.prettyName;

        var item=document.createElement('menuitem');
        item.setAttribute('label', identity.identityName);
        item.setAttribute('class', 'identity-popup-item');
        item.setAttribute('accountname', accountName);
        item.setAttribute('id', identity.key);
        popup.appendChild(item);
    }
}

function getCurrentIdentity()
{
    // fill in Identity combobox
    var identityList = document.getElementById("msgIdentity");

    var item = identityList.selectedItem;
    var identityKey = item.getAttribute('id');

    //dump("Looking for identity " + identityKey + "\n");
    var identity = gAccountManager.getIdentity(identityKey);

    return identity;
}

function getIdentityForKey(key)
{
    return gAccountManager.getIdentity(key);
}


function SuppressComposeCommandUpdating(suppress)
{
  gSuppressCommandUpdating = suppress;
  if (!gSuppressCommandUpdating)
    CommandUpdate_MsgCompose();
}

function AdjustFocus()
{
  //dump("XXX adjusting focus\n");
  SuppressComposeCommandUpdating(true);

  var element = document.getElementById("addressCol2#" + awGetNumberOfRecipients());
  if (element.value == "") {
      //dump("XXX focus on address\n");
      awSetFocus(awGetNumberOfRecipients(), element);
      //awSetFocus() will call SuppressComposeCommandUpdating(false);
  }
  else
  {
      element = document.getElementById("msgSubject");
      if (element.value == "") {
        //dump("XXX focus on subject\n");
        element.focus();
      }
      else {
        //dump("XXX focus on body\n");
        editorShell.contentWindow.focus();
      }
      SuppressComposeCommandUpdating(false);
  }
}

function SetComposeWindowTitle()
{
  var newTitle = document.getElementById('msgSubject').value;

  /* dump("newTitle = " + newTitle + "\n"); */

  if (newTitle == "" )
    newTitle = sComposeMsgsBundle.getString("defaultSubject");

  newTitle += GetCharsetUIString();
  window.title = sComposeMsgsBundle.getString("windowTitlePrefix") + " " + newTitle;
}

// Check for changes to document and allow saving before closing
// This is hooked up to the OS's window close widget (e.g., "X" for Windows)
function ComposeCanClose()
{
  if (gSendOrSaveOperationInProgress)
  {
    var result;

    if (gPromptService)
    {
      var promptTitle = sComposeMsgsBundle.getString("quitComposeWindowTitle");
      var promptMsg = sComposeMsgsBundle.getString("quitComposeWindowMessage");
      var quitButtonLabel = sComposeMsgsBundle.getString("quitComposeWindowQuitButtonLabel");
      var waitButtonLabel = sComposeMsgsBundle.getString("quitComposeWindowWaitButtonLabel");

      result = gPromptService.confirmEx(window, promptTitle, promptMsg,
          (gPromptService.BUTTON_TITLE_IS_STRING*gPromptService.BUTTON_POS_0) +
          (gPromptService.BUTTON_TITLE_IS_STRING*gPromptService.BUTTON_POS_1),
          waitButtonLabel, quitButtonLabel, null, null, {value:0});

      if (result == 1)
      {
        gMsgCompose.abort();
        return true;
      }
      return false;
    }
  }

  dump("XXX changed? " + gContentChanged + "," + gMsgCompose.bodyModified + "\n");
  // Returns FALSE only if user cancels save action
  if (gContentChanged || gMsgCompose.bodyModified)
  {
    // call window.focus, since we need to pop up a dialog
    // and therefore need to be visible (to prevent user confusion)
    window.focus();
    if (gPromptService)
    {
      result = gPromptService.confirmEx(window,
                              sComposeMsgsBundle.getString("saveDlogTitle"),
                              sComposeMsgsBundle.getString("saveDlogMessage"),
                              (gPromptService.BUTTON_TITLE_SAVE * gPromptService.BUTTON_POS_0) +
                              (gPromptService.BUTTON_TITLE_CANCEL * gPromptService.BUTTON_POS_1) +
                              (gPromptService.BUTTON_TITLE_DONT_SAVE * gPromptService.BUTTON_POS_2),
                              null, null, null,
                              null, {value:0});
      switch (result)
      {
        case 0: //Save
          if (LastToClose())
          NotifyQuitApplication();
          gCloseWindowAfterSave = true;
          SaveAsDraft();
          return false;
        case 1: //Cancel
          return false;
        case 2: //Don't Save
          break;
      }
    }

    SetContentAndBodyAsUnmodified();
  }

  return true;
}

function SetContentAndBodyAsUnmodified()
{
  gMsgCompose.bodyModified = false; 
  gContentChanged = false;
}

function MsgComposeCloseWindow(recycleIt)
{
  if (gMsgCompose)
    gMsgCompose.CloseWindow(recycleIt);
}

function GetLastAttachDirectory()
{
  var lastDirectory;

  try {
    lastDirectory = sPrefs.getComplexValue(kComposeAttachDirPrefName, Components.interfaces.nsILocalFile);
  }
  catch (ex) {
    // this will fail the first time we attach a file
    // as we won't have a pref value.
    lastDirectory = null;
  }

  return lastDirectory;
}

// attachedLocalFile must be a nsILocalFile
function SetLastAttachDirectory(attachedLocalFile)
{
  try {
    var file = attachedLocalFile.QueryInterface(Components.interfaces.nsIFile);
    var parent = file.parent.QueryInterface(Components.interfaces.nsILocalFile);
    
    sPrefs.setComplexValue(kComposeAttachDirPrefName, Components.interfaces.nsILocalFile, parent);
  }
  catch (ex) {
    dump("error: SetLastAttachDirectory failed: " + ex + "\n");
  }
}

function AttachFile()
{
  var attachments;
  
  //Get file using nsIFilePicker and convert to URL
  try {
      var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
      fp.init(window, sComposeMsgsBundle.getString("chooseFileToAttach"), nsIFilePicker.modeOpenMultiple);
      
      var lastDirectory = GetLastAttachDirectory();
      if (lastDirectory) 
        fp.displayDirectory = lastDirectory;

      fp.appendFilters(nsIFilePicker.filterAll);
      if (fp.show() == nsIFilePicker.returnOK) {
        attachments = fp.files;
      }
  }
  catch (ex) {
    dump("failed to get attachments: " + ex + "\n");
  }

  if (!attachments || !attachments.hasMoreElements())
    return;

  var haveSetAttachDirectory = false;

  while (attachments.hasMoreElements()) {
    var currentFile = attachments.getNext().QueryInterface(Components.interfaces.nsILocalFile);

    if (!haveSetAttachDirectory) {
      SetLastAttachDirectory(currentFile);
      haveSetAttachDirectory = true;
    }

    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
    ioService = ioService.getService(Components.interfaces.nsIIOService);
    var fileHandler = ioService.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
    var currentAttachment = fileHandler.getURLSpecFromFile(currentFile);

    if (!DuplicateFileCheck(currentAttachment)) {
      var attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].createInstance(Components.interfaces.nsIMsgAttachment);
      attachment.url = currentAttachment;
      AddAttachment(attachment);
      gContentChanged = true;
    }
  }
}

function AddAttachment(attachment)
{
  if (attachment && attachment.url)
  {
    var bucket = document.getElementById("attachmentBucket");
    var item = document.createElement("listitem");

    if (!attachment.name)
      attachment.name = gMsgCompose.AttachmentPrettyName(attachment.url);
    item.setAttribute("label", attachment.name);    //use for display only
    item.attachment = attachment;   //full attachment object stored here
    try {
      item.setAttribute("tooltiptext", unescape(attachment.url));
    } catch(e) {
      item.setAttribute("tooltiptext", attachment.url);
    }
    item.setAttribute("class", "listitem-iconic");
    item.setAttribute("image", "moz-icon:" + attachment.url);
    bucket.appendChild(item);
  }
}

function SelectAllAttachments()
{
  var bucketList = document.getElementById("attachmentBucket");
  if (bucketList)
    bucketList.selectAll();
}

function MessageHasAttachments()
{
  var bucketList = document.getElementById("attachmentBucket");
  if (bucketList) {
    return (bucketList && bucketList.hasChildNodes() && (bucketList == top.document.commandDispatcher.focusedElement));
  }
  return false;
}

function MessageHasSelectedAttachments()
{
  var bucketList = document.getElementById("attachmentBucket");

  if (bucketList)
    return (MessageHasAttachments() && bucketList.selectedItems && bucketList.selectedItems.length);
  return false;
}

function AttachPage()
{
   if (gPromptService)
   {
      var result = {value:"http://"};
      if (gPromptService.prompt(
        window,
        sComposeMsgsBundle.getString("attachPageDlogTitle"),
        sComposeMsgsBundle.getString("attachPageDlogMessage"),
          result,
        null,
        {value:0}))
      {
        var attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].createInstance(Components.interfaces.nsIMsgAttachment);
        attachment.url = result.value;
        AddAttachment(attachment);
      }
   }
}
function DuplicateFileCheck(FileUrl)
{
  var bucket = document.getElementById('attachmentBucket');
  for (var index = 0; index < bucket.childNodes.length; index++)
  {
    var item = bucket.childNodes[index];
    var attachment = item.attachment;
    if (attachment)
    {
      if (FileUrl == attachment.url)
         return true;
    }
  }

  return false;
}

function Attachments2CompFields(compFields)
{
  var bucket = document.getElementById('attachmentBucket');

  //First, we need to clear all attachment in the compose fields
  compFields.removeAttachments();

  for (var index = 0; index < bucket.childNodes.length; index++)
  {
    var item = bucket.childNodes[index];
    var attachment = item.attachment;
    if (attachment)
      compFields.addAttachment(attachment);
  }
}

function RemoveAllAttachments()
{
  var child;
	var bucket = document.getElementById("attachmentBucket");
  for (var i = bucket.childNodes.length - 1; i >= 0; i--)
  {
    child = bucket.removeChild(bucket.childNodes[i]);
    // Let's release the attachment object hold by the node else it won't go away until the window is destroyed
    child.attachment = null;
  }
}

function RemoveSelectedAttachment()
{
  var child;
  var bucket = document.getElementById("attachmentBucket");
  if (bucket.selectedItems.length > 0) {
    for (var item = bucket.selectedItems.length - 1; item >= 0; item-- )
    {
      child = bucket.removeChild(bucket.selectedItems[item]);
      // Let's release the attachment object hold by the node else it won't go away until the window is destroyed
      child.attachment = null;
    }
    gContentChanged = true;
  }
}

function FocusOnFirstAttachment()
{
  var bucketList = document.getElementById("attachmentBucket");

  if (bucketList && bucketList.hasChildNodes())
    bucketTree.selectItem(bucketList.firstChild);
}

function AttachmentElementHasItems()
{
  var element = document.getElementById("bucketList");

  return element ? element.childNodes.length : 0;
}  

function AttachVCard()
{
  dump("AttachVCard()\n");
}

function DetermineHTMLAction(convertible)
{
    var obj;
    if (! gMsgCompose.composeHTML)
    {
        try {
            obj = new Object;
            gMsgCompose.CheckAndPopulateRecipients(true, false, obj);
        } catch(ex) { dump("gMsgCompose.CheckAndPopulateRecipients failed: " + ex + "\n"); }
        return nsIMsgCompSendFormat.PlainText;
    }

    if (gSendFormat == nsIMsgCompSendFormat.AskUser)
    {
        //Well, before we ask, see if we can figure out what to do for ourselves

        var noHtmlRecipients;
        var noHtmlnewsgroups;
        var preferFormat;

        //Check the address book for the HTML property for each recipient
        try {
            obj = new Object;
            preferFormat = gMsgCompose.CheckAndPopulateRecipients(true, true, obj);
            noHtmlRecipients = obj.value;
        } catch(ex)
        {
            dump("gMsgCompose.CheckAndPopulateRecipients failed: " + ex + "\n");
            var msgCompFields = gMsgCompose.compFields;
            noHtmlRecipients = msgCompFields.to + "," + msgCompFields.cc + "," + msgCompFields.bcc;
            preferFormat = nsIAbPreferMailFormat.unknown;
        }
        dump("DetermineHTMLAction: preferFormat = " + preferFormat + ", noHtmlRecipients are " + noHtmlRecipients + "\n");

        //Check newsgroups now...
        try {
            noHtmlnewsgroups = gMsgCompose.GetNoHtmlNewsgroups(null);
        } catch(ex)
        {
           noHtmlnewsgroups = gMsgCompose.compFields.newsgroups;
        }

        if (noHtmlRecipients != "" || noHtmlnewsgroups != "")
        {
            if (convertible == nsIMsgCompConvertible.Plain)
              return nsIMsgCompSendFormat.PlainText;

            if (noHtmlnewsgroups == "")
            {
                switch (preferFormat)
                {
                  case nsIAbPreferMailFormat.plaintext :
                    return nsIMsgCompSendFormat.PlainText;

                  default :
                    //See if a preference has been set to tell us what to do. Note that we do not honor that
                    //preference for newsgroups. Only for e-mail addresses.
                    var action = sPrefs.getIntPref("mail.default_html_action");
                    switch (action)
                    {
                        case nsIMsgCompSendFormat.PlainText    :
                        case nsIMsgCompSendFormat.HTML         :
                        case nsIMsgCompSendFormat.Both         :
                            return action;
                    }
                }
            }
            return nsIMsgCompSendFormat.AskUser;
        }
        else
            return nsIMsgCompSendFormat.HTML;
    }
    else
    {
      try {
                       obj = new Object;
                       gMsgCompose.CheckAndPopulateRecipients(true, false, obj);
      } catch(ex) { dump("gMsgCompose.CheckAndPopulateRecipients failed: " + ex + "\n"); }
    }

    return gSendFormat;
}

function DetermineConvertibility()
{
    if (!gMsgCompose.composeHTML)
        return nsIMsgCompConvertible.Plain;

    try {
        return gMsgCompose.bodyConvertible();
    } catch(ex) {}
    return nsIMsgCompConvertible.No;
}

function LoadIdentity(startup)
{
    var identityElement = document.getElementById("msgIdentity");
    var prevIdentity = gCurrentIdentity;
    
    if (identityElement) {
        var item = identityElement.selectedItem;
        var idKey = item.getAttribute('id');
        gCurrentIdentity = gAccountManager.getIdentity(idKey);

        if (!startup && prevIdentity && idKey != prevIdentity.key)
        {
          var prefstring = "mail.identity." + prevIdentity.key;
          RemoveDirectoryServerObserver(prefstring);
          var prevReplyTo = prevIdentity.replyTo;
          var prevBcc = "";
          var prevReceipt = prevIdentity.requestReturnReceipt;
          if (prevIdentity.bccSelf)
            prevBcc += prevIdentity.email;
          if (prevIdentity.bccOthers)
          {
            if (prevBcc != "")
              prevBcc += ","
            prevBcc += prevIdentity.bccList;
          }

          var newReplyTo = gCurrentIdentity.replyTo;
          var newBcc = "";
          var newReceipt = gCurrentIdentity.requestReturnReceipt;
          if (gCurrentIdentity.bccSelf)
            newBcc += gCurrentIdentity.email;
          if (gCurrentIdentity.bccOthers)
          {
            if (newBcc != "")
              newBcc += ","
            newBcc += gCurrentIdentity.bccList;
          }

          var needToCleanUp = false;
          var msgCompFields = gMsgCompose.compFields;

          if (!gReceiptOptionChanged &&
              prevReceipt == msgCompFields.returnReceipt &&
              prevReceipt != newReceipt)
          {
            msgCompFields.returnReceipt = newReceipt;
            document.getElementById("returnReceiptMenu").setAttribute('checked',msgCompFields.returnReceipt);
          }


          if (newReplyTo != prevReplyTo)
          {
            needToCleanUp = true;
            if (prevReplyTo != "")
              awRemoveRecipients(msgCompFields, "addr_reply", prevReplyTo);
            if (newReplyTo != "")
              awAddRecipients(msgCompFields, "addr_reply", newReplyTo);
          }

          if (newBcc != prevBcc)
          {
            needToCleanUp = true;
            if (prevBcc != "")
              awRemoveRecipients(msgCompFields, "addr_bcc", prevBcc);
            if (newBcc != "")
              awAddRecipients(msgCompFields, "addr_bcc", newBcc);
          }

          if (needToCleanUp)
            awCleanupRows();

          try {
            gMsgCompose.SetSignature(gCurrentIdentity);
          } catch (ex) { dump("### Cannot set the signature: " + ex + "\n");}

          var event = document.createEvent('Events');
          event.initEvent('compose-from-changed', false, true);
          document.getElementById("msgcomposeWindow").dispatchEvent(event);
        }

      AddDirectoryServerObserver(false);
      if (!startup) {
          if (!gAutocompleteSession)
            gAutocompleteSession = Components.classes["@mozilla.org/autocompleteSession;1?type=addrbook"].getService(Components.interfaces.nsIAbAutoCompleteSession);
          if (gAutocompleteSession)
            setDomainName();

          try {
              setupLdapAutocompleteSession();
          } catch (ex) {
              // catch the exception and ignore it, so that if LDAP setup 
              // fails, the entire compose window doesn't end up horked
          }
      }
    }
}

function setDomainName()
{
  var emailAddr = gCurrentIdentity.email;
  var start = emailAddr.lastIndexOf("@");
  gAutocompleteSession.defaultDomain = emailAddr.slice(start + 1, emailAddr.length);
}

function setupAutocomplete()
{
  //Setup autocomplete session if we haven't done so already
  if (!gAutocompleteSession) {
    gAutocompleteSession = Components.classes["@mozilla.org/autocompleteSession;1?type=addrbook"].getService(Components.interfaces.nsIAbAutoCompleteSession);
    if (gAutocompleteSession) {
      setDomainName();

      // if the pref is set to turn on the comment column, honor it here.
      // this element then gets cloned for subsequent rows, so they should 
      // honor it as well
      //
      try {
          if (sPrefs.getIntPref("mail.autoComplete.commentColumn")) {
              document.getElementById('addressCol2#1').showCommentColumn =
                  true;
          }
      } catch (ex) {
          // if we can't get this pref, then don't show the columns (which is
          // what the XUL defaults to)
      }
              
    } else {
      gAutocompleteSession = 1;
    }
  }
  if (!gSetupLdapAutocomplete) {
      try {
          setupLdapAutocompleteSession();
      } catch (ex) {
          // catch the exception and ignore it, so that if LDAP setup 
          // fails, the entire compose window doesn't end up horked
      }
  }
}

function subjectKeyPress(event)
{  
  switch(event.keyCode) {
  case 9:
    if (!event.shiftKey) {
      window._content.focus();
      event.preventDefault();
    }
    break;
  case 13:
    window._content.focus();
    break;
  }
}

function editorKeyPress(event)
{
  if (event.keyCode == 9) {
    if (event.shiftKey) {
      document.getElementById('msgSubject').focus();
      event.preventDefault();
    }
  }
}

function AttachmentBucketClicked(event)
{
  if (event.button != 0)
    return;

  if (event.originalTarget.localName == "listboxbody")
    goDoCommand('cmd_attachFile');
}

var attachmentBucketObserver = {

  canHandleMultipleItems: true,

  onDrop: function (aEvent, aData, aDragSession)
    {
      var dataList = aData.dataList;
      var dataListLength = dataList.length;
      var errorTitle;
      var attachment;
      var errorMsg;

      for (var i = 0; i < dataListLength; i++) 
      {
        var item = dataList[i].first;
        var prettyName;
        var rawData = item.data;
        
        if (item.flavour.contentType == "text/x-moz-url" ||
            item.flavour.contentType == "text/x-moz-message" ||
            item.flavour.contentType == "application/x-moz-file")
        {
          if (item.flavour.contentType == "application/x-moz-file")
          {
            var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
            var fileHandler = ioService.getProtocolHandler("file")
                              .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
            rawData = fileHandler.getURLSpecFromFile(rawData);
          }
          else
          {
            var separator = rawData.indexOf("\n");
            if (separator != -1) 
            {
              prettyName = rawData.substr(separator+1);
              rawData = rawData.substr(0,separator);
            }
          }

          if (DuplicateFileCheck(rawData)) 
          {
            dump("Error, attaching the same item twice\n");
          }
          else 
          {
            attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"]
                         .createInstance(Components.interfaces.nsIMsgAttachment);
            attachment.url = rawData;
            attachment.name = prettyName;
            AddAttachment(attachment);
          }
        }
      }
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      var attachmentBucket = document.getElementById("attachmentBucket");
      attachmentBucket.setAttribute("dragover", "true");
    },

  onDragExit: function (aEvent, aDragSession)
    {
      var attachmentBucket = document.getElementById("attachmentBucket");
      attachmentBucket.removeAttribute("dragover");
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/x-moz-message");
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      return flavourSet;
    }
};

function DisplaySaveFolderDlg(folderURI)
{
  try{
    showDialog = gCurrentIdentity.showSaveMsgDlg;
  }//try
  catch (e){
    return;
  }//catch

  if (showDialog){
    var msgfolder = GetMsgFolderFromUri(folderURI, true);
    if (!msgfolder)
      return;
    var checkbox = {value:0};
    var SaveDlgTitle = sComposeMsgsBundle.getString("SaveDialogTitle");
    var dlgMsg = sComposeMsgsBundle.getFormattedString("SaveDialogMsg",
                                                       [msgfolder.name,
                                                        msgfolder.server.prettyName]);

    var CheckMsg = sComposeMsgsBundle.getString("CheckMsg");
    if (gPromptService)
      gPromptService.alertCheck(window, SaveDlgTitle, dlgMsg, CheckMsg, checkbox);
    else
      window.alert(dlgMsg);
    try {
          gCurrentIdentity.showSaveMsgDlg = !checkbox.value;
    }//try
    catch (e) {
    return;
    }//catch

  }//if
  return;
}



function SetMsgAddressingWidgetTreeElementFocus()
{
  SuppressComposeCommandUpdating(true);

  var element = document.getElementById("msgRecipient#" + awGetNumberOfRecipients());
  awSetFocus(awGetNumberOfRecipients(), element);
  //awSetFocus() will call SuppressComposeCommandUpdating(false);
}

function SetMsgIdentityElementFocus()
{
  // We're only changing focus from element to element.
  // There's no need to update the composer commands.
  SuppressComposeCommandUpdating(true);
  GetMsgIdentityElement().focus();
  SuppressComposeCommandUpdating(false);
}

function SetMsgSubjectElementFocus()
{
  SuppressComposeCommandUpdating(true);
  GetMsgSubjectElement().focus();
  SuppressComposeCommandUpdating(false);
}

function SetMsgAttachmentElementFocus()
{
  SuppressComposeCommandUpdating(true);
  GetMsgAttachmentElement().focus();
  FocusOnFirstAttachment();
  SuppressComposeCommandUpdating(false);
}

function SetMsgBodyFrameFocus()
{
  SuppressComposeCommandUpdating(true);
  editorShell.contentWindow.focus();
  SuppressComposeCommandUpdating(false);
}

function GetMsgAddressingWidgetTreeElement()
{
  if (!gMsgAddressingWidgetTreeElement)
    gMsgAddressingWidgetTreeElement = document.getElementById("addressingWidgetTree");

  return gMsgAddressingWidgetTreeElement;
}

function GetMsgIdentityElement()
{
  if (!gMsgIdentityElement)
    gMsgIdentityElement = document.getElementById("msgIdentity");

  return gMsgIdentityElement;
}

function GetMsgSubjectElement()
{
  if (!gMsgSubjectElement)
    gMsgSubjectElement = document.getElementById("msgSubject");

  return gMsgSubjectElement;
}

function GetMsgAttachmentElement()
{
  if (!gMsgAttachmentElement)
    gMsgAttachmentElement = document.getElementById("attachmentBucket");

  return gMsgAttachmentElement;
}

function GetMsgBodyFrame()
{
  if (!gMsgBodyFrame)
    gMsgBodyFrame = top.frames['browser.message.body'];

  return gMsgBodyFrame;
}

function GetMsgHeadersToolbarElement()
{
  if (!gMsgHeadersToolbarElement)
    gMsgHeadersToolbarElement = document.getElementById("MsgHeadersToolbar");

  return gMsgHeadersToolbarElement;
}

function WhichElementHasFocus()
{
  var msgIdentityElement             = GetMsgIdentityElement();
  var msgAddressingWidgetTreeElement = GetMsgAddressingWidgetTreeElement();
  var msgSubjectElement              = GetMsgSubjectElement();
  var msgAttachmentElement           = GetMsgAttachmentElement();
  var msgBodyFrame                   = GetMsgBodyFrame();

  if (top.document.commandDispatcher.focusedWindow == msgBodyFrame)
    return msgBodyFrame;

  var currentNode = top.document.commandDispatcher.focusedElement;
  while (currentNode)
  {
    if (currentNode == msgIdentityElement ||
        currentNode == msgAddressingWidgetTreeElement ||
        currentNode == msgSubjectElement ||
        currentNode == msgAttachmentElement)
      return currentNode;

    currentNode = currentNode.parentNode;
  }

  return null;
}

// Function that performs the logic of switching focus from 
// one element to another in the mail compose window.
// The default element to switch to when going in either
// direction (shift or no shift key pressed), is the
// AddressingWidgetTreeElement.
//
// The only exception is when the MsgHeadersToolbar is
// collapsed, then the focus will always be on the body of
// the message.
function SwitchElementFocus(event)
{
  var focusedElement = WhichElementHasFocus();

  if (event && event.shiftKey)
  {
    if (focusedElement == gMsgAddressingWidgetTreeElement)
      SetMsgIdentityElementFocus();
    else if (focusedElement == gMsgIdentityElement)
      SetMsgBodyFrameFocus();
    else if (focusedElement == gMsgBodyFrame)
    {
      // only set focus to the attachment element if there
      // are any attachments.
      if (AttachmentElementHasItems())
        SetMsgAttachmentElementFocus();
      else
        SetMsgSubjectElementFocus();
    }
    else if (focusedElement == gMsgAttachmentElement)
      SetMsgSubjectElementFocus();
    else
      SetMsgAddressingWidgetTreeElementFocus();
  }
  else
  {
    if (focusedElement == gMsgAddressingWidgetTreeElement)
      SetMsgSubjectElementFocus();
    else if (focusedElement == gMsgSubjectElement)
    {
      // only set focus to the attachment element if there
      // are any attachments.
      if (AttachmentElementHasItems())
        SetMsgAttachmentElementFocus();
      else
        SetMsgBodyFrameFocus();
    }
    else if (focusedElement == gMsgAttachmentElement)
      SetMsgBodyFrameFocus();
    else if (focusedElement == gMsgBodyFrame)
      SetMsgIdentityElementFocus();
    else
      SetMsgAddressingWidgetTreeElementFocus();
  }
}

