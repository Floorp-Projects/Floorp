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

var msgCompDeliverMode = Components.interfaces.nsIMsgCompDeliverMode;
var msgCompSendFormat = Components.interfaces.nsIMsgCompSendFormat;
var msgCompConvertible = Components.interfaces.nsIMsgCompConvertible;
var msgCompType = Components.interfaces.nsIMsgCompType;
var msgCompFormat = Components.interfaces.nsIMsgCompFormat;
var abPreferMailFormat = Components.interfaces.nsIAbPreferMailFormat;
var plaintextEditor = Components.interfaces.nsIPlaintextEditor;

var accountManagerContractID   = "@mozilla.org/messenger/account-manager;1";
var accountManager = Components.classes[accountManagerContractID].getService(Components.interfaces.nsIMsgAccountManager);

var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                           .getService(Components.interfaces.nsIIOService);

//var mailSessionContractID   = "@mozilla.org/messenger/services/session;1";
//var mailSession = Components.classes[mailSessionContractID].getService(Components.interfaces.nsIMsgMailSession);

var messengerMigratorContractID   = "@mozilla.org/messenger/migrator;1";

var msgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService();
msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

// We are storing the value of the bool logComposePerformance inorder to avoid logging unnecessarily.
var gLogComposePerformance = msgComposeService.logComposePerformance;

var gPromptService = null;

//This migrates the LDAPServer Preferences from 4.x to mozilla format.
try {
  var LDAPPrefsService = Components.classes[
                       "@mozilla.org/ldapprefs-service;1"].getService();
  LDAPPrefsService = LDAPPrefsService.QueryInterface(
                   Components.interfaces.nsILDAPPrefsService);
} catch (ex) {dump ("ERROR:" + ex + "\n");}

var msgCompose = null;
var MAX_RECIPIENTS = 0;
var currentAttachment = null;
var gWindowLocked = false;
var contentChanged = false;
var gCurrentIdentity = null;
var defaultSaveOperation = "draft";
var sendOrSaveOperationInProgress = false;
var gCloseWindowAfterSave = false;
var isOffline = false;
var sessionAdded = false;
var currentAutocompleteDirectory = null;
var gAutocompleteSession = null;
var gSetupLdapAutocomplete = false;
var gLDAPSession = null;
var gComposeMsgsBundle;
var gSavedSendNowKey;

const DEBUG = false;

var other_header = "";
var sendFormat = msgCompSendFormat.AskUser;
var prefs = null;
var gPrefBranchInternal = null;

var gLastElementToHaveFocus;  
var gSuppressCommandUpdating = false;

function disableEditableFields()
{
  editorShell.editor.SetFlags(plaintextEditor.eEditorReadonlyMask);
  var disableElements = document.getElementsByAttribute("disableonsend", "true");
  for (i=0;i<disableElements.length;i++)
  {
    disableElements[i].setAttribute('disabled', 'true');
  }
}

function enableEditableFields()
{
  editorShell.editor.SetFlags(plaintextEditor.eEditorMailMask);
  var enableElements = document.getElementsByAttribute("disableonsend", "true");
  for (i=0;i<enableElements.length;i++)
  {
    enableElements[i].removeAttribute('disabled');
  }
}

var stateListener = {
  NotifyComposeFieldsReady: function() {
    ComposeFieldsReady();
  },

  ComposeProcessDone: function(aResult) {
    dump("\n RECEIVE ComposeProcessDone\n\n");
    gWindowLocked = false;
    CommandUpdate_MsgCompose();
    enableEditableFields();

    if (aResult== Components.results.NS_OK)
    {
      contentChanged = false;
      msgCompose.bodyModified = false;
     
      if (gCloseWindowAfterSave)
        msgCompose.CloseWindow();
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
        sendOrSaveOperationInProgress = false;
        document.getElementById('compose-progressmeter').setAttribute( "mode", "normal" );
        document.getElementById('compose-progressmeter').setAttribute( "value", 0 );
        setTimeout("document.getElementById('statusText').setAttribute('label', '')", 5000);
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
     if (iid.equals(Components.interfaces.nsIWebProgressListener) || iid.equals(Components.interfaces.nsISupportsWeakReference))
      return this;
     
     throw Components.results.NS_NOINTERFACE;
    }
};

// i18n globals
var g_currentMailSendCharset = null;
var g_send_default_charset = null;
var g_charsetTitle = null;
var g_charsetConvertManager = Components.classes['@mozilla.org/charset-converter-manager;1'].getService();
g_charsetConvertManager = g_charsetConvertManager.QueryInterface(Components.interfaces.nsICharsetConverterManager2);
  //Create message window object
var msgWindowContractID      = "@mozilla.org/messenger/msgwindow;1";
var msgWindow = Components.classes[msgWindowContractID].createInstance();

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
//      case "cmd_quoteMessage":
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

    var composeHTML = msgCompose && msgCompose.composeHTML;

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
        return !(gWindowLocked || isOffline);
      case "cmd_quit":
        return true;

      //Edit Menu
      case "cmd_pasteQuote":
      case "cmd_find":
      case "cmd_findNext":
        //Disable the editor specific edit commands if the focus is not into the body
        return !focusedElement;
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
        return !focusedElement;
      case "cmd_outputFormat":
        return composeHTML;
//      case "cmd_quoteMessage":
//        return mailSession && mailSession.topmostMsgWindow;
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
          if (ioService && ioService.offline)
            SendMessageLater();
          else
            SendMessage();
        }
        break;
      case "cmd_sendNow"            : if (defaultController.isCommandEnabled(command)) SendMessage();          break;
      case "cmd_sendWithCheck"   : if (defaultController.isCommandEnabled(command)) SendMessageWithCheck();          break;
      case "cmd_sendLater"          : if (defaultController.isCommandEnabled(command)) SendMessageLater();     break;
      case "cmd_printSetup"         : goPageSetup();                                                           break;
      case "cmd_print"              : DoCommandPrint();                                                        break;

      //Edit Menu
      case "cmd_account"            : MsgAccountManager(null); break;
      case "cmd_preferences"        : DoCommandPreferences(); break;

      //View Menu
      case "cmd_showComposeToolbar" : goToggleToolbar('composeToolbar', 'menu_showComposeToolbar'); break;
      case "cmd_showFormatToolbar"  : goToggleToolbar('FormatToolbar', 'menu_showFormatToolbar');   break;

      //Options Menu
      case "cmd_selectAddress"      : if (defaultController.isCommandEnabled(command)) SelectAddress();         break;
//      case "cmd_quoteMessage"       : if (defaultController.isCommandEnabled(command)) QuoteSelectedMessage();  break;
      case "cmd_rewrap"             : editorShell.Rewrap(false);                                                break;

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
  if (msgCompose && msgCompose.composeHTML)
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
  goUpdateCommand("cmd_find");
  goUpdateCommand("cmd_findNext");
}

var messageComposeOfflineObserver = {
  observe: function(subject, topic, state) {
    // sanity checks
    if (topic != "network:offline-status-changed") return;
    if (state == "offline")
      isOffline = true;
    else
      isOffline = false;
    MessageComposeOfflineStateChanged(isOffline);

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
  
  isOffline = ioService.offline;
  // set the initial state of the send button
  MessageComposeOfflineStateChanged(isOffline);
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
    gPrefBranchInternal.addObserver("ldap_2.autoComplete.useDirectory",
                                    directoryServerObserver, false);
    gPrefBranchInternal.addObserver("ldap_2.autoComplete.directoryServer",
                                    directoryServerObserver, false);
  }
  else
  {
    var prefstring = "mail.identity." + gCurrentIdentity.key + ".overrideGlobal_Pref";
    gPrefBranchInternal.addObserver(prefstring, directoryServerObserver, false);
    prefstring = "mail.identity." + gCurrentIdentity.key + ".directoryServer";
    gPrefBranchInternal.addObserver(prefstring, directoryServerObserver, false);
  }
}

function RemoveDirectoryServerObserver(prefstring)
{
  if (!prefstring) {
    gPrefBranchInternal.removeObserver("ldap_2.autoComplete.useDirectory", directoryServerObserver);
    gPrefBranchInternal.removeObserver("ldap_2.autoComplete.directoryServer", directoryServerObserver);
  }
  else
  {
    var str = prefstring + ".overrideGlobal_Pref";
    gPrefBranchInternal.removeObserver(str, directoryServerObserver);
    str = prefstring + ".directoryServer";
    gPrefBranchInternal.removeObserver(str, directoryServerObserver);
  }
}

function AddDirectorySettingsObserver()
{
  gPrefBranchInternal.addObserver(currentAutocompleteDirectory, directoryServerObserver, false);
}

function RemoveDirectorySettingsObserver(prefstring)
{
  gPrefBranchInternal.removeObserver(prefstring, directoryServerObserver);
}

function setupLdapAutocompleteSession()
{
    var autocompleteLdap = false;
    var autocompleteDirectory = null;
    var prevAutocompleteDirectory = currentAutocompleteDirectory;
    var i;

    autocompleteLdap = prefs.getBoolPref("ldap_2.autoComplete.useDirectory");
    if (autocompleteLdap)
        autocompleteDirectory = prefs.getCharPref(
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
            
    if (autocompleteDirectory && !isOffline) { 
        // Add observer on the directory server we are autocompleting against
        // only if current server is different from previous.
        // Remove observer if current server is different from previous       
        currentAutocompleteDirectory = autocompleteDirectory;
        if (prevAutocompleteDirectory) {
          if (prevAutocompleteDirectory != currentAutocompleteDirectory) { 
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
                serverURL.spec = prefs.getCharPref(autocompleteDirectory + 
                                                    ".uri");
            } catch (ex) {
                dump("ERROR: " + ex + "\n");
            }
            LDAPSession.serverURL = serverURL;

            // don't search on non-CJK strings shorter than this
            //
            try { 
                LDAPSession.minStringLength = prefs.getIntPref(
                    autocompleteDirectory + ".autoComplete.minStringLength");
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default.
            }

            // don't search on CJK strings shorter than this
            //
            try { 
                LDAPSession.cjkMinStringLength = prefs.getIntPref(
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
                    prefs.getComplexValue(autocompleteDirectory + 
                                      ".autoComplete.nameFormat",
                                      Components.interfaces.nsISupportsWString).data;
            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsAbLDAPAutoCompFormatter use its default.
            }

            // override autocomplete mail address format?
            //
            try {
                ldapFormatter.addressFormat = 
                    prefs.getComplexValue(autocompleteDirectory + 
                                      ".autoComplete.addressFormat",
                                      Components.interfaces.nsISupportsWString).data;
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
                showComments = prefs.getIntPref(
                    "mail.autoComplete.commentColumn");

                switch (showComments) {

                case 1:
                    // use the name of this directory
                    //
                    ldapFormatter.commentFormat = prefs.getComplexValue(
                                autocompleteDirectory + ".description",
                                Components.interfaces.nsISupportsWString).data;
                    break;

                case 2:
                    // override ldap-specific autocomplete entry?
                    //
                    try {
                        ldapFormatter.commentFormat = 
                            prefs.getComplexValue(autocompleteDirectory + 
                                        ".autoComplete.commentFormat",
                                        Components.interfaces.nsISupportsWString).data;
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
                    prefs.getComplexValue(autocompleteDirectory + 
                                      ".autoComplete.outputFormat",
                                      Components.interfaces.nsISupportsWString).data;

            } catch (ex) {
                // if this pref isn't there, no big deal.  just let
                // nsLDAPAutoCompleteSession use its default.
            }

            // override default search filter template?
            //
            try { 
                LDAPSession.filterTemplate = prefs.getComplexValue(
                    autocompleteDirectory + ".autoComplete.filterTemplate",
                    Components.interfaces.nsISupportsWString).data;

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
                    prefs.getIntPref(autocompleteDirectory + ".maxHits");
            } catch (ex) {
                // if this pref isn't there, or is out of range, no big deal. 
                // just let nsLDAPAutoCompleteSession use its default.
            }

            if (!sessionAdded) {
                // if we make it here, we know that session initialization has
                // succeeded; add the session for all recipients, and 
                // remember that we've done so
                var autoCompleteWidget;
                for (i=1; i <= MAX_RECIPIENTS; i++)
                {
                    autoCompleteWidget = document.getElementById("msgRecipient#" + i);
                    if (autoCompleteWidget)
                    {
                      autoCompleteWidget.addSession(LDAPSession);
                      // ldap searches don't insert a default entry with the default domain appended to it
                      // so reduce the minimum results for a popup to 2 in this case. 
                      autoCompleteWidget.minResultsForPopup = 2;

                    }
                 }
                sessionAdded = true;
            }
        }
    } else {
      if (currentAutocompleteDirectory) {
        // Remove observer on the directory server since we are not doing Ldap
        // autocompletion.
        RemoveDirectorySettingsObserver(currentAutocompleteDirectory);
        currentAutocompleteDirectory = null;
      }
      if (gLDAPSession && sessionAdded) {
        for (i=1; i <= MAX_RECIPIENTS; i++) 
          document.getElementById("msgRecipient#" + i).
              removeSession(gLDAPSession);
        sessionAdded = false;
      }
    }

    gLDAPSession = LDAPSession;
    gSetupLdapAutocomplete = true;
}

function DoCommandClose()
{
  var retVal;
  if ((retVal = ComposeCanClose()))
  MsgComposeCloseWindow();
  return retVal;
}

function DoCommandPrint()
{
  if (msgCompose)
  {
    var editorShell = msgCompose.editor;
    if (editorShell)
      try {
        editorShell.FinishHTMLSource();
        editorShell.Print();
      } catch(ex) {dump("#PRINT ERROR: " + ex + "\n");}
  }
}

function DoCommandPreferences()
{
  goPreferences('messengercompose.xul', 'chrome://messenger/content/messengercompose/pref-composing_messages.xul')
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
  if (! msgCompose.composeHTML) {
    try {
      window.editorShell.wrapColumn = msgCompose.wrapLength;
    }
    catch (e) {
      dump("### window.editorShell.wrapColumn exception text: " + e + " - failed\n");
    }
  }
  CompFields2Recipients(msgCompose.compFields, msgCompose.type);
  SetComposeWindowTitle(13);
  AdjustFocus();
}

function ComposeStartup()
{
  var params = null; // New way to pass parameters to the compose window as a nsIMsgComposeParameters object
  var args = null;   // old way, parameters are passed as a string
  if (window.arguments && window.arguments[0]) {
    try {
      params = window.arguments[0].QueryInterface(Components.interfaces.nsIMsgComposeParams);
    }
    catch(ex) {
    }
    if (!params)
      args = GetArgs(window.arguments[0]);
  }

  var identityList = document.getElementById("msgIdentity");
  var identityListPopup = document.getElementById("msgIdentityPopup");

  if (identityListPopup) {
    fillIdentityListPopup(identityListPopup);
  }

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
    var identities = accountManager.defaultAccount.identities;
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
  if (msgComposeService)
  {
    msgCompose = msgComposeService.InitCompose(window, params);
    if (msgCompose)
    {
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

      // save the editorShell in the window. The editor JS expects to find it there.
      window.editorShell = editorShell;
//      dump("Created editorShell\n");

      // setEditorType MUST be call before setContentWindow
      if (msgCompose.composeHTML)
      {
        window.editorShell.editorType = "htmlmail";
      }
      else
      {
        //Remove HTML toolbar, format and insert menus as we are editing in plain text mode
        document.getElementById("outputFormatMenu").setAttribute("hidden", true);
        document.getElementById("FormatToolbar").setAttribute("hidden", true);
        document.getElementById("formatMenu").setAttribute("hidden", true);
        document.getElementById("insertMenu").setAttribute("hidden", true);
        document.getElementById("menu_showFormatToolbar").setAttribute("hidden", true);

        window.editorShell.editorType = "textmail";
//        dump("editor initialized in PLAIN TEXT mode\n");
      }
      window.editorShell.webShellWindow = window;
      window.editorShell.contentWindow = window._content;

      // Do setup common to Message Composer and Web Composer
      EditorSharedStartup();

      var msgCompFields = msgCompose.compFields;
      if (msgCompFields)
      {
        if (params.bodyIsLink)
        {
          var body = msgCompFields.body;
          if (msgCompose.composeHTML)
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
        if (subjectValue != "") {
          document.getElementById("msgSubject").value = subjectValue;
        }

        var attachments = msgCompFields.attachmentsArray;
        if (attachments)
          for (var i = 0; i < attachments.Count(); i ++)
            AddAttachment(attachments.QueryElementAt(i, Components.interfaces.nsIMsgAttachment));
        }

      // Now that we have an Editor AppCore, we can finish to initialize the Compose AppCore
      msgCompose.editor = window.editorShell;

      msgCompose.RegisterStateListener(stateListener);


    }
  }
}
function WizCallback(state)
{
  if (state){
    ComposeStartup();
  }
  else
  {
    if (msgCompose)
      msgCompose.CloseWindow();
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
    prefs = prefService.getBranch(null);
    gPrefBranchInternal = prefs.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
  }
  catch (ex) {
    dump("failed to preferences services\n");
  }

  try {
    other_header = prefs.getCharPref("mail.compose.other.header");
  }
  catch (ex) {
    dump("failed to get the mail.compose.other.header pref\n");
  }

  AddMessageComposeOfflineObserver();
  AddDirectoryServerObserver(true);

  if (gLogComposePerformance)
    msgComposeService.TimeStamp("Start initializing the compose window (ComposeLoad)", false);
  gComposeMsgsBundle = document.getElementById("bundle_composeMsgs");

  try {
    SetupCommandUpdateHandlers();
    var wizardcallback = true;
    var state = verifyAccounts(wizardcallback); // this will do migration, or create a new account if we need to.

    if (other_header != "") {
      var selectNode = document.getElementById('msgRecipientType#1');
      selectNode = selectNode.childNodes[0];
      var opt = document.createElement('menuitem');
      opt.setAttribute("value", "addr_other");
      opt.setAttribute("label", other_header + ":");
      selectNode.appendChild(opt);
    }
    if (state)
      ComposeStartup();
  }
  catch (ex) {
    var errorTitle = gComposeMsgsBundle.getString("initErrorDlogTitle");
    var errorMsg = gComposeMsgsBundle.getFormattedString("initErrorDlogMessage",
                                                         [ex]);
    if (!gPromptService) {
      gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
      gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
    }
    if (gPromptService)
      gPromptService.alert(window, errorTitle, errorMsg);
    else
      window.alert(errorMsg);

    if (msgCompose)
      msgCompose.CloseWindow();
    else
      window.close();
    return;
  }
  window.tryToClose=ComposeCanClose;
  if (gLogComposePerformance)
    msgComposeService.TimeStamp("Done with the initialization (ComposeLoad). Waiting on editor to load about:blank", false);
}

function ComposeUnload()
{
  dump("\nComposeUnload from XUL\n");
  RemoveMessageComposeOfflineObserver();
    RemoveDirectoryServerObserver(null);
    RemoveDirectoryServerObserver("mail.identity." + gCurrentIdentity.key);
    if (currentAutocompleteDirectory)
       RemoveDirectorySettingsObserver(currentAutocompleteDirectory);
  msgCompose.UnregisterStateListener(stateListener);
}

function SetDocumentCharacterSet(aCharset)
{
  dump("SetDocumentCharacterSet Callback!\n");
  dump(aCharset + "\n");

  if (msgCompose) {
    msgCompose.SetDocumentCharset(aCharset);
    g_currentMailSendCharset = aCharset;
    g_charsetTitle = null;
    SetComposeWindowTitle(13);
  }
  else
    dump("Compose has not been created!\n");
}

function UpdateMailEditCharset()
{
  var send_default_charset = prefs.getComplexValue("mailnews.send_default_charset",
                                     Components.interfaces.nsIPrefLocalizedString).data;
//  dump("send_default_charset is " + send_default_charset + "\n");

  var compFieldsCharset = msgCompose.compFields.characterSet;
//  dump("msgCompose.compFields is " + compFieldsCharset + "\n");

  if (g_charsetConvertManager) {
    var charsetAtom = g_charsetConvertManager.GetCharsetAtom(compFieldsCharset);
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
  if (g_currentMailSendCharset != null) {
    dump("already set to " + g_currentMailSendCharset + "\n");
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
  var charset = msgCompose.compFields.characterSet;
  if (g_send_default_charset == null) {
    g_send_default_charset = prefs.getComplexValue("mailnews.send_default_charset",
                                     Components.interfaces.nsIPrefLocalizedString).data;
  }

  charset = charset.toUpperCase();
  if (charset == "US-ASCII")
    charset = "ISO-8859-1";

  if (charset != g_send_default_charset) {

    if (g_charsetTitle == null) {
      try {
        // check if we have a converter for this charset
        var charsetAtom = g_charsetConvertManager.GetCharsetAtom(charset);
        var encoderList = g_charsetConvertManager.GetEncoderList();
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
          msgCompose.compFields.characterSet = g_send_default_charset;
          return "";
        }

        // get a localized string
        g_charsetTitle = g_charsetConvertManager.GetCharsetTitle(charsetAtom);
      }
      catch (ex) {
        dump("failed to get a charset title of " + charset + "!\n");
        g_charsetTitle = charset; // just show the charset itself
      }
    }

    return " - " + g_charsetTitle;
  }

  return "";
}

function GenericSendMessage( msgType )
{
  dump("GenericSendMessage from XUL\n");

  dump("Identity = " + getCurrentIdentity() + "\n");

  if (msgCompose != null)
  {
      var msgCompFields = msgCompose.compFields;
      if (msgCompFields)
      {
      Recipients2CompFields(msgCompFields);
      var subject = document.getElementById("msgSubject").value;
      msgCompFields.subject = subject;
      Attachments2CompFields(msgCompFields);

      if (msgType == msgCompDeliverMode.Now || msgType == msgCompDeliverMode.Later)
      {
        //Do we need to check the spelling?
        if (prefs.getBoolPref("mail.SpellCheckBeforeSend"))
          goDoCommand('cmd_spelling');

        //Check if we have a subject, else ask user for confirmation
        if (subject == "")
        {
        if (!gPromptService) {
          gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
                                  gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
                                }
            if (gPromptService)
            {
            var result = {value:gComposeMsgsBundle.getString("defaultSubject")};
                if (gPromptService.prompt(
                  window,
                  gComposeMsgsBundle.getString("subjectDlogTitle"),
                  gComposeMsgsBundle.getString("subjectDlogMessage"),
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
        if (action == msgCompSendFormat.AskUser)
        {
                    var recommAction = convert == msgCompConvertible.No
                                   ? msgCompSendFormat.AskUser
                                   : msgCompSendFormat.PlainText;
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
          case msgCompSendFormat.PlainText:
            msgCompFields.forcePlainText = true;
            msgCompFields.useMultipartAlternative = false;
            break;
          case msgCompSendFormat.HTML:
            msgCompFields.forcePlainText = false;
            msgCompFields.useMultipartAlternative = false;
            break;
          case msgCompSendFormat.Both:
            msgCompFields.forcePlainText = false;
            msgCompFields.useMultipartAlternative = true;
            break;
           default: dump("\###SendMessage Error: invalid action value\n"); return;
        }
      }
      try {
        gWindowLocked = true;
        CommandUpdate_MsgCompose();
        disableEditableFields();

        var progress = Components.classes["@mozilla.org/messenger/progress;1"].createInstance(Components.interfaces.nsIMsgProgress);
        if (progress)
        {
          progress.registerListener(progressListener);
          sendOrSaveOperationInProgress = true;
        }
        msgCompose.SendMsg(msgType, getCurrentIdentity(), progress);
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
  GenericSendMessage(msgCompDeliverMode.Now);
}

function SendMessageWithCheck()
{
    var warn = prefs.getBoolPref("mail.warn_on_send_accel_key");

    if (warn) {
        var buttonPressed = {value:1};
        var checkValue = {value:false};
        if (!gPromptService) {
            gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
            gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
        }
        gPromptService.confirmEx(window, 
              gComposeMsgsBundle.getString('sendMessageCheckWindowTitle'), 
              gComposeMsgsBundle.getString('sendMessageCheckLabel'),
              (gPromptService.BUTTON_TITLE_IS_STRING * gPromptService.BUTTON_POS_0) +
              (gPromptService.BUTTON_TITLE_CANCEL * gPromptService.BUTTON_POS_1),
              gComposeMsgsBundle.getString('sendMessageCheckSendButtonLabel'),
              null, null,
              gComposeMsgsBundle.getString('CheckMsg'), 
              checkValue, buttonPressed);
        if (!buttonPressed || buttonPressed.value != 0) {
            return;
        }
        if (checkValue.value) {
            prefs.setBoolPref("mail.warn_on_send_accel_key", false);
        }
    }

  GenericSendMessage(isOffline ? msgCompDeliverMode.Later
                                 : msgCompDeliverMode.Now);
}

function SendMessageLater()
{
  dump("SendMessageLater from XUL\n");
  GenericSendMessage(msgCompDeliverMode.Later);
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
  if (msgCompose.bodyConvertible() == msgCompConvertible.Plain)
    editorShell.saveDocument(saveAs, false, "text/plain");
  else
    editorShell.saveDocument(saveAs, false, "text/html");
  defaultSaveOperation = "file";
}

function SaveAsDraft()
{
  dump("SaveAsDraft from XUL\n");

  GenericSendMessage(msgCompDeliverMode.SaveAsDraft);
  defaultSaveOperation = "draft";
}

function SaveAsTemplate()
{
  dump("SaveAsTemplate from XUL\n");

  GenericSendMessage(msgCompDeliverMode.SaveAsTemplate);
  defaultSaveOperation = "template";
}


function MessageFcc(menuItem)
{
  // Get the id for the folder we're FCC into
  // This is the additional FCC in addition to the
  // default FCC
  destUri = menuItem.getAttribute('id');
  if (msgCompose)
  {
    var msgCompFields = msgCompose.compFields;
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
  dump("Set Message Priority to " + target.getAttribute('id') + "\n");
  if (msgCompose)
  {
    var msgCompFields = msgCompose.compFields;
    if (msgCompFields)
      msgCompFields.priority = target.getAttribute('id');
  }
}

function ReturnReceiptMenuSelect()
{
  if (msgCompose)
  {
    var msgCompFields = msgCompose.compFields;
    if (msgCompFields)
    {
      if (msgCompFields.returnReceipt)
      {
        msgCompFields.returnReceipt = false;
      }
      else
      {
        msgCompFields.returnReceipt = true;
      }
    }
  }
}

function OutputFormatMenuSelect(target)
{
  dump("Set Message Format to " + target.getAttribute('id') + "\n");
  if (msgCompose)
  {
    var msgCompFields = msgCompose.compFields;

        if (msgCompFields)
        {
            switch (target.getAttribute('id'))
          {
            case "1": sendFormat = msgCompSendFormat.AskUser;     break;
            case "2": sendFormat = msgCompSendFormat.PlainText;   break;
            case "3": sendFormat = msgCompSendFormat.HTML;        break;
            case "4": sendFormat = msgCompSendFormat.Both;        break;
          }
        }
  }
}

function SelectAddress()
{
  var msgCompFields = msgCompose.compFields;

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

    var idSupports = accountManager.allIdentities;
    var identities = queryISupportsArray(idSupports,
                                         Components.interfaces.nsIMsgIdentity);

    dump(identities + "\n");
    return identities;
}

function fillIdentityListPopup(popup)
{
    var identities = GetIdentities();

    for (var i=0; i<identities.length; i++)
    {
    var identity = identities[i];

    //dump(i + " = " + identity.identityName + "," +identity.key + "\n");

    //Get server prettyName for each identity

    var serverSupports = accountManager.GetServersForIdentity(identity);

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
    var identity = accountManager.getIdentity(identityKey);

    return identity;
}

function getIdentityForKey(key)
{
    return accountManager.getIdentity(key);
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

  var element = document.getElementById("msgRecipient#" + awGetNumberOfRecipients());
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

function SetComposeWindowTitle(event)
{
  /* dump("event = " + event + "\n"); */

  /* only set the title when they hit return (or tab?)
   */

  var newTitle = document.getElementById('msgSubject').value;

  /* dump("newTitle = " + newTitle + "\n"); */

  if (newTitle == "" ) {
    newTitle = gComposeMsgsBundle.getString("defaultSubject");
  }

  newTitle += GetCharsetUIString();

  window.title = gComposeMsgsBundle.getString("windowTitlePrefix") + " " + newTitle;
}

// Check for changes to document and allow saving before closing
// This is hooked up to the OS's window close widget (e.g., "X" for Windows)
function ComposeCanClose()
{
  if (!gPromptService) {
    gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
  }
  if (sendOrSaveOperationInProgress)
  {
      var result;

      if (gPromptService)
      {
        var promptTitle = gComposeMsgsBundle.getString("quitComposeWindowTitle");
        var promptMsg = gComposeMsgsBundle.getString("quitComposeWindowMessage");
        var quitButtonLabel = gComposeMsgsBundle.getString("quitComposeWindowQuitButtonLabel");
        var waitButtonLabel = gComposeMsgsBundle.getString("quitComposeWindowWaitButtonLabel");

        result = {value:0};
        gPromptService.confirmEx(window, promptTitle, promptMsg,
          (gPromptService.BUTTON_TITLE_IS_STRING*gPromptService.BUTTON_POS_0) +
          (gPromptService.BUTTON_TITLE_IS_STRING*gPromptService.BUTTON_POS_1),
          waitButtonLabel, quitButtonLabel, null, null, {value:0}, result);

        if (result.value == 1)
          {
            msgCompose.abort();
            return true;
          }
        else 
          {
            return false;
          }
      }
  }
  // Returns FALSE only if user cancels save action
  if (contentChanged || msgCompose.bodyModified)
  {
    // call window.focus, since we need to pop up a dialog
    // and therefore need to be visible (to prevent user confusion)
    window.focus();
    if (gPromptService)
    {
            result = {value:0};
            gPromptService.confirmEx(window,
                              gComposeMsgsBundle.getString("saveDlogTitle"),
                              gComposeMsgsBundle.getString("saveDlogMessage"),
                              (gPromptService.BUTTON_TITLE_SAVE * gPromptService.BUTTON_POS_0) +
                              (gPromptService.BUTTON_TITLE_CANCEL * gPromptService.BUTTON_POS_1) +
                              (gPromptService.BUTTON_TITLE_DONT_SAVE * gPromptService.BUTTON_POS_2),
                              null, null, null,
                              null, {value:0}, result);

      if (result)
      {
        switch (result.value)
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
    }

    msgCompose.bodyModified = false;
    contentChanged = false;
  }

  return true;
}

function MsgComposeCloseWindow()
{
  if (msgCompose)
    msgCompose.CloseWindow();
}

function AttachFile()
{
//  dump("AttachFile()\n");
  currentAttachment = "";
  //Get file using nsIFilePicker and convert to URL
    try {
      var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
      fp.init(window, gComposeMsgsBundle.getString("chooseFileToAttach"), nsIFilePicker.modeOpen);
      fp.appendFilters(nsIFilePicker.filterAll);
      if (fp.show() == nsIFilePicker.returnOK) {
      currentAttachment = fp.fileURL.spec;
      dump("nsIFilePicker - "+currentAttachment+"\n");
      }
    }
  catch (ex) {
    dump("failed to get the local file to attach\n");
  }
  if (currentAttachment == "")
    return;

  if (!(DuplicateFileCheck(currentAttachment)))
  {
    var attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].createInstance(Components.interfaces.nsIMsgAttachment);
    attachment.url = currentAttachment;
    AddAttachment(attachment);
  }
  else
  {
    dump("###ERROR ADDING DUPLICATE FILE \n");
    var errorTitle = gComposeMsgsBundle.getString("DuplicateFileErrorDlogTitle");
    var errorMsg = gComposeMsgsBundle.getString("DuplicateFileErrorDlogMessage");
    if (!gPromptService) {
      gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
      gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
    }
    if (gPromptService)
      gPromptService.alert(window, errorTitle, errorMsg);
    else
      window.alert(errorMsg);
  }
}

function AddAttachment(attachment)
{
  if (attachment && attachment.url)
  {
    var bucketBody = document.getElementById("bucketBody");
    var item = document.createElement("treeitem");
    var row = document.createElement("treerow");
    var cell = document.createElement("treecell");

    if (!attachment.name)
      attachment.name = msgCompose.AttachmentPrettyName(attachment.url);
    cell.setAttribute("label", attachment.name);    //use for display only
    cell.attachment = attachment;   //full attachment object stored here
    cell.setAttribute("tooltip", "aTooltip");
    try {
      cell.setAttribute("tooltiptext", unescape(attachment.url));
    }
    catch(e) {cell.setAttribute("tooltiptext", attachment.url);}
    cell.setAttribute("class", "treecell-iconic");
    cell.setAttribute('src', "moz-icon:" + attachment.url);
    row.appendChild(cell);
    item.appendChild(row);
    bucketBody.appendChild(item);
  }
}


function AttachPage()
{
     if (!gPromptService) {
       gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
       gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
     }
     
     if (gPromptService)
     {
        var result = {value:"http://"};
        if (gPromptService.prompt(
          window,
          gComposeMsgsBundle.getString("attachPageDlogTitle"),
          gComposeMsgsBundle.getString("attachPageDlogMessage"),
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
  var body = document.getElementById('bucketBody');
  var item, row, cell, colon;
  var attachment;

  for (var index = 0; index < body.childNodes.length; index++)
  {
    item = body.childNodes[index];
    if (item.childNodes && item.childNodes.length)
    {
      row = item.childNodes[0];
      if (row.childNodes &&  row.childNodes.length)
      {
        cell = row.childNodes[0];
        if (cell)
        {
          attachment = cell.attachment;
          if (attachment)
          {
            if (FileUrl == attachment.url)
               return true;
          }
        }
      }
    }
  }

  return false;
}

function Attachments2CompFields(compFields)
{
  var body = document.getElementById('bucketBody');
  var item, row, text, colon;
  var attachment;

  //First, we need to clear all attachment in the compose fields
  compFields.removeAttachments();

  for (var index = 0; index < body.childNodes.length; index++)
  {
    item = body.childNodes[index];
    if (item.childNodes && item.childNodes.length)
    {
      row = item.childNodes[0];
      if (row.childNodes &&  row.childNodes.length)
      {
        cell = row.childNodes[0];
        if (cell)
          {
          attachment = cell.attachment;
          if (attachment)
            compFields.addAttachment(attachment);
        }
      }
    }
  }
}

function RemoveSelectedAttachment()
{
  var bucketTree = document.getElementById("attachmentBucket");
  if ( bucketTree )
  {
    var body = document.getElementById("bucketBody");

    if ( body && bucketTree.selectedItems && bucketTree.selectedItems.length )
    {
      for ( var item = bucketTree.selectedItems.length - 1; item >= 0; item-- )
        body.removeChild(bucketTree.selectedItems[item]);
    }
  }

}

function AttachVCard()
{
  dump("AttachVCard()\n");
}

function DetermineHTMLAction(convertible)
{
    var obj;
    if (! msgCompose.composeHTML)
    {
        try {
            obj = new Object;
            msgCompose.CheckAndPopulateRecipients(true, false, obj);
        } catch(ex) { dump("msgCompose.CheckAndPopulateRecipients failed: " + ex + "\n"); }
        return msgCompSendFormat.PlainText;
    }

    if (sendFormat == msgCompSendFormat.AskUser)
    {
        //Well, before we ask, see if we can figure out what to do for ourselves

        var noHtmlRecipients;
        var noHtmlnewsgroups;
        var preferFormat;

        //Check the address book for the HTML property for each recipient
        try {
            obj = new Object;
            preferFormat = msgCompose.CheckAndPopulateRecipients(true, true, obj);
            noHtmlRecipients = obj.value;
        } catch(ex)
        {
            dump("msgCompose.CheckAndPopulateRecipients failed: " + ex + "\n");
            var msgCompFields = msgCompose.compFields;
            noHtmlRecipients = msgCompFields.to + "," + msgCompFields.cc + "," + msgCompFields.bcc;
            preferFormat = abPreferMailFormat.unknown;
        }
        dump("DetermineHTMLAction: preferFormat = " + preferFormat + ", noHtmlRecipients are " + noHtmlRecipients + "\n");

        //Check newsgroups now...
        try {
            noHtmlnewsgroups = msgCompose.GetNoHtmlNewsgroups(null);
        } catch(ex)
        {
           noHtmlnewsgroups = msgCompose.compFields.newsgroups;
        }

        if (noHtmlRecipients != "" || noHtmlnewsgroups != "")
        {
            if (convertible == msgCompConvertible.Plain)
              return msgCompSendFormat.PlainText;

            if (noHtmlnewsgroups == "")
            {
                switch (preferFormat)
                {
                  case abPreferMailFormat.plaintext :
                    return msgCompSendFormat.PlainText;

                  default :
                    //See if a preference has been set to tell us what to do. Note that we do not honor that
                    //preference for newsgroups. Only for e-mail addresses.
                    var action = prefs.getIntPref("mail.default_html_action");
                    switch (action)
                    {
                        case msgCompSendFormat.PlainText    :
                        case msgCompSendFormat.HTML         :
                        case msgCompSendFormat.Both         :
                            return action;
                    }
                }
            }
            return msgCompSendFormat.AskUser;
        }
        else
            return msgCompSendFormat.HTML;
    }
    else
    {
      try {
                       obj = new Object;
                       msgCompose.CheckAndPopulateRecipients(true, false, obj);
      } catch(ex) { dump("msgCompose.CheckAndPopulateRecipients failed: " + ex + "\n"); }
    }

    return sendFormat;
}

function DetermineConvertibility()
{
    if (!msgCompose.composeHTML)
        return msgCompConvertible.Plain;

    try {
        return msgCompose.bodyConvertible();
    } catch(ex) {}
    return msgCompConvertible.No;
}

function LoadIdentity(startup)
{
    var identityElement = document.getElementById("msgIdentity");
    var prevIdentity = gCurrentIdentity;
    
    if (identityElement) {
        var item = identityElement.selectedItem;
        var idKey = item.getAttribute('id');
        gCurrentIdentity = accountManager.getIdentity(idKey);

        if (!startup && prevIdentity && idKey != prevIdentity.key)
        {
          var prefstring = "mail.identity." + prevIdentity.key;
          RemoveDirectoryServerObserver(prefstring);
          var prevReplyTo = prevIdentity.replyTo;
          var prevBcc = "";
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
          if (gCurrentIdentity.bccSelf)
            newBcc += gCurrentIdentity.email;
          if (gCurrentIdentity.bccOthers)
          {
            if (newBcc != "")
              newBcc += ","
            newBcc += gCurrentIdentity.bccList;
          }

          var needToCleanUp = false;
          var msgCompFields = msgCompose.compFields;

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
            msgCompose.SetSignature(gCurrentIdentity);
          } catch (ex) { dump("### Cannot set the signature: " + ex + "\n");}
        }

      AddDirectoryServerObserver(false);
      if (!startup) {
          try {
              setupLdapAutocompleteSession();
          } catch (ex) {
              // catch the exception and ignore it, so that if LDAP setup 
              // fails, the entire compose window doesn't end up horked
          }
      }
    }
}


function setupAutocomplete()
{
  //Setup autocomplete session if we haven't done so already
  if (!gAutocompleteSession) {
    gAutocompleteSession = Components.classes["@mozilla.org/autocompleteSession;1?type=addrbook"].getService(Components.interfaces.nsIAbAutoCompleteSession);
    if (gAutocompleteSession) {
      var emailAddr = gCurrentIdentity.email;
      var start = emailAddr.lastIndexOf("@");
      gAutocompleteSession.defaultDomain = emailAddr.slice(start + 1, emailAddr.length);

      // if the pref is set to turn on the comment column, honor it here.
      // this element then gets cloned for subsequent rows, so they should 
      // honor it as well
      //
      try {
          if (prefs.getIntPref("mail.autoComplete.commentColumn")) {
              document.getElementById('msgRecipient#1').showCommentColumn =
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
  if (event.originalTarget.localName == 'treechildren')
    goDoCommand('cmd_attachFile');
}

var attachmentBucketObserver = {
  onDrop: function (aEvent, aData, aDragSession)
    {
      var prettyName;
      var rawData = aData.data;
      switch (aData.flavour.contentType) {
      case "text/x-moz-url":
      case "text/nsmessageOrFolder":
        var separator = rawData.indexOf("\n");
        if (separator != -1) {
          prettyName = rawData.substr(separator+1);
          rawData = rawData.substr(0,separator);
        }
        break;
      case "application/x-moz-file":
        rawData = aData.data.URL;
        break;
      }
      if (!(DuplicateFileCheck(rawData)))
      {
        var attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].createInstance(Components.interfaces.nsIMsgAttachment);
        attachment.url = rawData;
        attachment.name = prettyName;
        AddAttachment(attachment);
      }
      else {
        var errorTitle = gComposeMsgsBundle.getString("DuplicateFileErrorDlogTitle");
        var errorMsg = gComposeMsgsBundle.getString("DuplicateFileErrorDlogMessage");
        if (!gPromptService) {
          gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
          gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
        }
        if (gPromptService)
          gPromptService.alert(window, errorTitle, errorMsg);
        else
          window.alert(errorMsg);
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
      flavourSet.appendFlavour("text/nsmessageOrFolder");
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      return flavourSet;
    }
};

function GetMsgFolderFromUri(uri)
{
  try {
    var RDF = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService();
    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
    var resource = RDF.GetResource(uri);
		var msgfolder = resource.QueryInterface(Components.interfaces.nsIMsgFolder);
        if (msgfolder && ( msgfolder.parent || msgfolder.isServer))
		  return msgfolder;
	}//try
	catch (ex) { }//catch
	return null;
}

function DisplaySaveFolderDlg(folderURI)
{
  try{
    showDialog = gCurrentIdentity.showSaveMsgDlg;
  }//try
  catch (e){
    return;
  }//catch

  if (showDialog){
    var msgfolder = GetMsgFolderFromUri(folderURI);
    if (!msgfolder)
      return;
    var checkbox = {value:0};
    var SaveDlgTitle = gComposeMsgsBundle.getString("SaveDialogTitle");
    var dlgMsg = gComposeMsgsBundle.getFormattedString("SaveDialogMsg",
                                                       [msgfolder.name,
                                                        msgfolder.hostname]);

    var CheckMsg = gComposeMsgsBundle.getString("CheckMsg");
    if (!gPromptService) {
      gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
      gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
    }
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

