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
var msgCompType = Components.interfaces.nsIMsgCompType;

var accountManagerProgID   = "component://netscape/messenger/account-manager";
var accountManager = Components.classes[accountManagerProgID].getService(Components.interfaces.nsIMsgAccountManager);

//var mailSessionProgID   = "component://netscape/messenger/services/session";
//var mailSession = Components.classes[mailSessionProgID].getService(Components.interfaces.nsIMsgMailSession);

var messengerMigratorProgID   = "component://netscape/messenger/migrator";

var msgComposeService = Components.classes["component://netscape/messengercompose"].getService();
msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

var commonDialogsService = Components.classes["component://netscape/appshell/commonDialogs"].getService();
commonDialogsService = commonDialogsService.QueryInterface(Components.interfaces.nsICommonDialogs);

var msgCompose = null;
var MAX_RECIPIENTS = 0;
var currentAttachment = null;
var documentLoaded = false;
var windowLocked = false;
var contentChanged = false;
var currentIdentity = null;
var defaultSaveOperation = "draft";

var Bundle = srGetStrBundle("chrome://messenger/locale/messengercompose/composeMsgs.properties"); 

var other_header = "";
var update_compose_title_as_you_type = true;
var sendFormat = msgCompSendFormat.AskUser;
var prefs = Components.classes["component://netscape/preferences"].getService();
if (prefs) {
	prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
	if (prefs) {
		try {
			update_compose_title_as_you_type = prefs.GetBoolPref("mail.update_compose_title_as_you_type");
		}
		catch (ex) {
			dump("failed to get the mail.update_compose_title_as_you_type pref\n");
		}
		try {
			other_header = prefs.CopyCharPref("mail.compose.other.header");
		}
		catch (ex) {
			 dump("failed to get the mail.compose.other.header pref\n");
		}
	}
}

var stateListener = {
	NotifyComposeFieldsReady: function() {
		documentLoaded = true;
	},

	SendAndSaveProcessDone: function() {
		dump("\n RECEIVE SaveAndSendProcessDone\n\n");
		windowLocked = false;
	  CommandUpdate_MsgCompose();
	}
};

// i18n globals
var currentMailSendCharset = null;
var g_send_default_charset = null;
var g_charsetTitle = null;


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
      case "cmd_sendNow":
      case "cmd_sendLater":
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
      case "cmd_format":
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
      case "cmd_sendNow":
      case "cmd_sendLater":
        return !windowLocked;
      case "cmd_quit":
        return true;

      //Edit Menu
      case "cmd_pasteQuote":
      case "cmd_find":
      case "cmd_findNext":
        //Disable the editor specific edit commands if the focus is not into the body
        return /*!focusedElement*/false;
      case "cmd_account":
      case "cmd_preferences":
        return true;

      //View Menu
      case "cmd_showComposeToolbar":
        return true;
      case "cmd_showFormatToolbar":
        return msgCompose.composeHTML;

      //Insert Menu
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
        return /*!focusedElement*/false;
    
      //Options Menu
      case "cmd_selectAddress":
        return !windowLocked;
      case "cmd_spelling":
        return !focusedElement;
      case "cmd_outputFormat":
        return msgCompose.composeHTML;
//      case "cmd_quoteMessage":
//        return mailSession && mailSession.topmostMsgWindow;
      
      //Format Menu
      case "cmd_format":
        return !focusedElement;
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
        return /*!focusedElement*/false;

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
      case "cmd_sendNow"            : if (defaultController.isCommandEnabled(command)) SendMessage();          break;
      case "cmd_sendLater"          : if (defaultController.isCommandEnabled(command)) SendMessageLater();     break;
      case "cmd_account"            : MsgAccountManager();    break;
      case "cmd_preferences"        : DoCommandPreferences(); break;

      //View Menu
      case "cmd_showComposeToolbar" : goToggleToolbar('composeToolbar', 'menu_showComposeToolbar'); break;
      case "cmd_showFormatToolbar"  : goToggleToolbar('FormatToolbar', 'menu_showFormatToolbar');   break;

      //Options Menu
      case "cmd_selectAddress"      : if (defaultController.isCommandEnabled(command)) SelectAddress();         break;
//      case "cmd_quoteMessage"       : if (defaultController.isCommandEnabled(command)) QuoteSelectedMessage();  break;

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
//  dump("\nCommandUpdate_MsgCompose\n");
  try {
  
  //File Menu
  goUpdateCommand("cmd_attachFile");
  goUpdateCommand("cmd_attachPage");
  goUpdateCommand("cmd_close");
  goUpdateCommand("cmd_saveDefault");
  goUpdateCommand("cmd_saveAsFile");
  goUpdateCommand("cmd_saveAsDraft");
  goUpdateCommand("cmd_saveAsTemplate");
  goUpdateCommand("cmd_sendNow");
  goUpdateCommand("cmd_sendLater");
  goUpdateCommand("cmd_quit");

  //Edit Menu
  goUpdateCommand("cmd_pasteQuote");
  goUpdateCommand("cmd_find");
  goUpdateCommand("cmd_findNext");
  goUpdateCommand("cmd_account");
  goUpdateCommand("cmd_preferences");

  //View Menu
  goUpdateCommand("cmd_showComposeToolbar");
  goUpdateCommand("cmd_showFormatToolbar");
  
  //Insert Menu
  if (msgCompose.composeHTML)
  {
    goUpdateCommand("cmd_insert");
    goUpdateCommand("cmd_link");
    goUpdateCommand("cmd_anchor");
    goUpdateCommand("cmd_image");
    goUpdateCommand("cmd_hline");
    goUpdateCommand("cmd_table");
    goUpdateCommand("cmd_insertHTML");
    goUpdateCommand("cmd_insertChars");
    goUpdateCommand("cmd_insertBreak");
    goUpdateCommand("cmd_insertBreakAll");
  }
  
  //Format Menu
  if (msgCompose.composeHTML)
  {
    goUpdateCommand("cmd_format");
    goUpdateCommand("cmd_decreaseFont");
    goUpdateCommand("cmd_increaseFont");
    goUpdateCommand("cmd_bold");
    goUpdateCommand("cmd_italic");
    goUpdateCommand("cmd_underline");
    goUpdateCommand("cmd_strikethrough");
    goUpdateCommand("cmd_superscript");
    goUpdateCommand("cmd_subscript");
    goUpdateCommand("cmd_nobreak");
    goUpdateCommand("cmd_em");
    goUpdateCommand("cmd_strong");
    goUpdateCommand("cmd_cite");
    goUpdateCommand("cmd_abbr");
    goUpdateCommand("cmd_acronym");
    goUpdateCommand("cmd_code");
    goUpdateCommand("cmd_samp");
    goUpdateCommand("cmd_var");
    goUpdateCommand("cmd_removeList");
    goUpdateCommand("cmd_ul");
    goUpdateCommand("cmd_ol");
    goUpdateCommand("cmd_dt");
    goUpdateCommand("cmd_dd");
    goUpdateCommand("cmd_listProperties");
    goUpdateCommand("cmd_indent");
    goUpdateCommand("cmd_outdent");
    goUpdateCommand("cmd_objectProperties");
    goUpdateCommand("cmd_InsertTable");
    goUpdateCommand("cmd_InsertRowAbove");
    goUpdateCommand("cmd_InsertRowBelow");
    goUpdateCommand("cmd_InsertColumnBefore");
    goUpdateCommand("cmd_InsertColumnAfter");
    goUpdateCommand("cmd_SelectTable");
    goUpdateCommand("cmd_SelectRow");
    goUpdateCommand("cmd_SelectColumn");
    goUpdateCommand("cmd_SelectCell");
    goUpdateCommand("cmd_SelectAllCells");
    goUpdateCommand("cmd_DeleteTable");
    goUpdateCommand("cmd_DeleteRow");
    goUpdateCommand("cmd_DeleteColumn");
    goUpdateCommand("cmd_DeleteCell");
    goUpdateCommand("cmd_DeleteCellContents");
    goUpdateCommand("cmd_NormalizeTable");
    goUpdateCommand("cmd_tableJoinCells");
    goUpdateCommand("cmd_tableSplitCell");
    goUpdateCommand("cmd_editTable");
  }

  //Options Menu
  goUpdateCommand("cmd_selectAddress");
  goUpdateCommand("cmd_spelling");
  goUpdateCommand("cmd_outputFormat");
//  goUpdateCommand("cmd_quoteMessage");
  
  } catch(e) {}
}

function DoCommandClose()
{
  if (ComposeCanClose())
    CloseWindow()
}

function DoCommandPreferences()
{
  goPreferences('messengercompose.xul', 'chrome://messenger/content/messengercompose/pref-composing_messages.xul')
}

function ToggleWindowLock()
{
  windowLocked = !windowLocked;
  CommandUpdate_MsgCompose();
}

function GetArgs()
{
	var args = new Object();
	var originalData = document.getElementById("args").getAttribute("value");
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
//	dump("Compose: argument: {" + data + "}\n");

	for (var i = pairs.length - 1; i >= 0; i--)
	{
		var pos = pairs[i].indexOf('=');
		if (pos == -1)
			continue;
		var argname = pairs[i].substring(0, pos);
		var argvalue = pairs[i].substring(pos + 1);
		if (argvalue.charAt(0) == "'" && argvalue.charAt(argvalue.length - 1) == "'")
			args[argname] = argvalue.substring(1, argvalue.length - 1);
		else
			args[argname] = unescape(argvalue);
		dump("[" + argname + "=" + args[argname] + "]\n");
	}
	return args;
}

function WaitFinishLoadingDocument(msgType)
{
	if (documentLoaded)
	{
	    //If we are in plain text, we nee to set the wrap column
		if (! msgCompose.composeHTML)
    		try
    		{
    			window.editorShell.wrapColumn = msgCompose.wrapLength;
    		}
    		catch (e)
    		{
    			dump("### window.editorShell.wrapColumn exception text: " + e + " - failed\n");
    		}
		    
		CompFields2Recipients(msgCompose.compFields, msgType);
		SetComposeWindowTitle(13);
		AdjustFocus();
		try {
		window.updateCommands("create");
		} catch(e) {}
	}
	else
		setTimeout("WaitFinishLoadingDocument(" + msgType + ");", 200);
}

function ComposeStartup()
{
	dump("Compose: ComposeStartup\n");

	//dump("Get arguments\n");
	var args = GetArgs();

    //dump("fill in Identity menulist\n");
    var identityList = document.getElementById("msgIdentity");
    var identityListPopup = document.getElementById("msgIdentityPopup");

    if (identityListPopup) {
        fillIdentityListPopup(identityListPopup);
    }

    var identity = null;
    if (args.preselectid)
        identity = getIdentityForKey(args.preselectid);
    else
    {
       // no preselect, so use the default account
       var identities = accountManager.defaultAccount.identities;      
      if (identities.Count() >= 1)
    	  identity = identities.QueryElementAt(0, Components.interfaces.nsIMsgIdentity);
    	else
    	{
        var identities = GetIdentities();
        identity = identities[0];
      }	  
    }
   
    for (i=0;i<identityListPopup.childNodes.length;i++) {
        var item = identityListPopup.childNodes[i];
        var id = item.getAttribute('id');
        if (id == identity.key) {
            identityList.selectedItem = item;
            break;
        }
    }
    LoadIdentity(true);

	if (msgComposeService)
	{
        // this is frustrating, we need to convert the preselect identity key
        // back to an identity, to pass to initcompose
        // it would be nice if there was some way to actually send the
        // identity through "args"
        
		msgCompose = msgComposeService.InitCompose(window, args.originalMsg, args.type, args.format, args.fieldsAddr, identity);
		if (msgCompose)
		{
			//Creating a Editor Shell
      var editorShell = Components.classes["component://netscape/editor/editorshell"].createInstance();
      editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
			if (!editorShell)
			{
				dump("Failed to create editorShell!\n");
				return;
			}
			
			// save the editorShell in the window. The editor JS expects to find it there.
			window.editorShell = editorShell;
			window.editorShell.Init();
			dump("Created editorShell\n");

			// setEditorType MUST be call before setContentWindow
			if (msgCompose.composeHTML)
			{
				window.editorShell.editorType = "htmlmail";
				dump("editor initialized in HTML mode\n");
			}
			else
			{
			    //Remove HTML toolbar, format and insert menus as we are editing in plain text mode
			    document.getElementById("FormatToolbar").setAttribute("hidden", true);
			    document.getElementById("formatMenu").setAttribute("hidden", true);
			    document.getElementById("insertMenu").setAttribute("hidden", true);
			    document.getElementById("menu_showFormatToolbar").setAttribute("checked", false);


				window.editorShell.editorType = "textmail";
				dump("editor initialized in PLAIN TEXT mode\n");
			}
			window.editorShell.webShellWindow = window;
			window.editorShell.contentWindow = window._content;

      // Do setup common to Message Composer and Web Composer
      EditorSharedStartup();

	    	var msgCompFields = msgCompose.compFields;
	    	if (msgCompFields)
	    	{
	    		if (args.body) //We need to set the body before setting
                               //msgCompose.editor;
                {
                    if (args.bodyislink == "true")
					{
						if (msgCompose.composeHTML)
							msgCompFields.SetBody("<BR><A HREF=\"" + args.body +
													  "\">" + unescape(args.body)
													  + "</A><BR>");
						else
							msgCompFields.SetBody("\n<" + args.body + ">\n");
					}
                    else
                        msgCompFields.SetBody(args.body);
                }

	    		if (args.to)
	    			msgCompFields.SetTo(args.to);
	    		if (args.cc)
	    			msgCompFields.SetCc(args.cc);
	    		if (args.bcc)
	    			msgCompFields.SetBcc(args.bcc);
	    		if (args.newsgroups)
	    			msgCompFields.SetNewsgroups(args.newsgroups);
	    		if (args.subject)
	    			msgCompFields.SetSubject(args.subject);
	    		if (args.attachment)
	    			msgCompFields.SetAttachments(args.attachment);
				dump("args newshost = " + args.newshost + "\n");
				if (args.newshost)
					msgCompFields.SetNewshost(args.newshost);
			
				var subjectValue = msgCompFields.GetSubject();
				if (subjectValue != "") {
					document.getElementById("msgSubject").value = subjectValue;
				}
                var attachmentValue = msgCompFields.GetAttachments();
                if (attachmentValue != "") {
                   var atts =  attachmentValue.split(",");
                    for (var i=0; i < atts.length; i++)
                    {
                        AddAttachment(atts[i]);
                    }
                }
			}
 			
			// Now that we have an Editor AppCore, we can finish to initialize the Compose AppCore
			msgCompose.editor = window.editorShell;
			
			//We need to unregister some of the editor commands to avoid conflict
			if (gHTMLEditorCommandManager)
			  gHTMLEditorCommandManager.unregisterCommand("cmd_preferences", nsPreferencesCommand);

			msgCompose.RegisterStateListener(stateListener);

			// call updateCommands to disable while we're loading the page
		  try {
			window.updateCommands("create");
  		} catch(e) {}
			
//	    SetupCommandUpdateHandlers();
			WaitFinishLoadingDocument(args.type);
		}
	}
}

function ComposeLoad()
{
	dump("\nComposeLoad from XUL\n");
	
	SetupCommandUpdateHandlers();

	verifyAccounts();	// this will do migration, if we need to.

	var selectNode = document.getElementById('msgRecipientType#1');

	if (other_header != "") {
		var opt = new Option(other_header + ":", "addr_other");
		selectNode.add(opt, null); 
	}

    // See if we got arguments.
    if ( window.arguments && window.arguments[0] != null ) {
        // Window was opened via window.openDialog.  Copy argument
        // and perform compose initialization (as if we were
        // opened via toolkitCore's ShowWindowWithArgs).
        document.getElementById( "args" ).setAttribute( "value", window.arguments[0] );
        ComposeStartup();
    }
	window.tryToClose=ComposeCanClose;

}

function ComposeUnload(calledFromExit)
{
	dump("\nComposeUnload from XUL\n");
	
	msgCompose.UnregisterStateListener(stateListener);
	if (msgCompose && msgComposeService)
		msgComposeService.DisposeCompose(msgCompose, false);
	//...and what's about the editor appcore, how can we release it?
}

function SetDocumentCharacterSet(aCharset)
{
  dump("SetDocumentCharacterSet Callback!\n");
  dump(aCharset + "\n");

  if (msgCompose) {
    msgCompose.SetDocumentCharset(aCharset);
    currentMailSendCharset = aCharset;
    g_charsetTitle = null;
    SetComposeWindowTitle(13);
  }
  else
    dump("Compose has not been created!\n");
}

function SetDefaultMailSendCharacterSet()
{
  // Set the current menu selection as the default
  if (currentMailSendCharset != null) {
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

	  if (msgCompose) {
      // write to the pref file
      prefs.SetCharPref("mailnews.send_default_charset", currentMailSendCharset);
      // update the global
      g_send_default_charset = currentMailSendCharset;
      dump("Set send_default_charset to" + currentMailSendCharset + "\n");
    }
	  else
		  dump("Compose has not been created!\n");
  }
}

function InitCharsetMenuCheckMark()
{
  // dump("msgCompose.compFields is " + msgCompose.compFields.GetCharacterSet() + "\n");
  // return if the charset is already set explitily
  if (currentMailSendCharset != null) {
    dump("already set to " + currentMailSendCharset + "\n");
    return;
  }

  var menuitem; 

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
  var send_default_charset = prefs.getLocalizedUnicharPref("mailnews.send_default_charset");
//  send_default_charset = send_default_charset.toUpperCase();
  dump("send_default_charset is " + send_default_charset + "\n");

  var compFieldsCharset = msgCompose.compFields.GetCharacterSet();
//  compFieldsCharset = compFieldsCharset.toUpperCase();
  dump("msgCompose.compFields is " + compFieldsCharset + "\n");

  if (compFieldsCharset == "us-ascii")
    compFieldsCharset = "ISO-8859-1";
  menuitem = document.getElementById(compFieldsCharset);

  // charset may have been set implicitly in case of reply/forward
  if (send_default_charset != compFieldsCharset) {
    menuitem.setAttribute('checked', 'true');
    return;
  } 

  // use pref default
  menuitem = document.getElementById(send_default_charset);
  if (menuitem) 
    menuitem.setAttribute('checked', 'true'); 

  // Set a document charset to a default mail send charset.
  SetDocumentCharacterSet(send_default_charset);
}

function GetCharsetUIString()
{
  var charset = msgCompose.compFields.GetCharacterSet();
  if (g_send_default_charset == null) {
    try {
      prefs = Components.classes['component://netscape/preferences'];
      prefs = prefs.getService();
      prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
      g_send_default_charset = prefs.getLocalizedUnicharPref("mailnews.send_default_charset");
    }
    catch (ex) {
      dump("failed to get prefs service!\n");
      prefs = null;
      g_send_default_charset = charset; // set to the current charset
    }
  }

  charset = charset.toUpperCase();
  if (charset == "US-ASCII")
    charset = "ISO-8859-1";

  if (charset != g_send_default_charset) {

    if (g_charsetTitle == null) {
      try {
        var ccm = Components.classes['component://netscape/charset-converter-manager'];
        ccm = ccm.getService();
        ccm = ccm.QueryInterface(Components.interfaces.nsICharsetConverterManager2);
        // get a localized string
        var charsetAtom = ccm.GetCharsetAtom(charset);
        g_charsetTitle = ccm.GetCharsetTitle(charsetAtom);
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
			msgCompFields.SetSubject(subject);
			dump("attachments = " + GenerateAttachmentsString() + "\n");
			try {
				msgCompFields.SetAttachments(GenerateAttachmentsString());
			}
			catch (ex) {
				dump("failed to SetAttachments\n");
			}
		
			if (msgType == msgCompDeliverMode.Now || msgType == msgCompDeliverMode.Later)
			{
			  //Do we need to check the spelling?
        if (prefs.GetBoolPref("mail.SpellCheckBeforeSend"))
	        goDoCommand('cmd_spelling');
			  
				//Check if we have a subject, else ask user for confirmation
				if (subject == "")
				{ 
    				if (commonDialogsService)
    				{
						var result = {value:0};
        				if (commonDialogsService.Prompt(
        					window,
        					Bundle.GetStringFromName("subjectDlogTitle"),
        					Bundle.GetStringFromName("subjectDlogMessage"),
        					Bundle.GetStringFromName("defaultSubject"),
        					result
        					))
        				{
        					msgCompFields.SetSubject(result.value);
        					var subjectInputElem = document.getElementById("msgSubject");
        					subjectInputElem.value = result.value;
        				}
        				else
        				  return;
        			}
    			}
            
				// Before sending the message, check what to do with HTML message, eventually abort.
				action = DetermineHTMLAction();
				if (action == msgCompSendFormat.AskUser)
				{
					var result = {action:msgCompSendFormat.PlainText, abort:false};
					window.openDialog("chrome://messenger/content/messengercompose/askSendFormat.xul",
										"askSendFormatDialog", "chrome,modal",
										result);
					if (result.abort)
						return;
					 action = result.action;
				}
				switch (action)
				{
					case msgCompSendFormat.PlainText:
						msgCompFields.SetTheForcePlainText(true);
						msgCompFields.SetUseMultipartAlternativeFlag(false);
						break;
					case msgCompSendFormat.HTML:
						msgCompFields.SetTheForcePlainText(false);
						msgCompFields.SetUseMultipartAlternativeFlag(false);
						break;
					case msgCompSendFormat.Both:
						msgCompFields.SetTheForcePlainText(false);
						msgCompFields.SetUseMultipartAlternativeFlag(true);
						break;	    
				   default: dump("\###SendMessage Error: invalid action value\n"); return;
				}
			}
			try {
			  windowLocked = true;
			  CommandUpdate_MsgCompose();
				msgCompose.SendMsg(msgType, getCurrentIdentity(), null);
				contentChanged = false;
				msgCompose.bodyModified = false;
			}
			catch (ex) {
				dump("failed to SendMsg\n");
			  windowLocked = false;
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
  // 0 = nsMsgDeliverNow
  // RICHIE: We should really have a way of using constants and not
  // hardcoded numbers for the first argument
	GenericSendMessage(msgCompDeliverMode.Now);
}

function SendMessageLater()
{
	dump("SendMessageLater from XUL\n");
  // 1 = nsMsgQueueForLater
  // RICHIE: We should really have a way of using constants and not
  // hardcoded numbers for the first argument
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
	editorShell.saveDocument(saveAs, false);
  defaultSaveOperation = "file";
}

function SaveAsDraft()
{
	dump("SaveAsDraft from XUL\n");

  // 4 = nsMsgSaveAsDraft
  // RICHIE: We should really have a way of using constants and not
  // hardcoded numbers for the first argument
  GenericSendMessage(msgCompDeliverMode.SaveAsDraft);
  defaultSaveOperation = "draft";
}

function SaveAsTemplate()
{
	dump("SaveAsTemplate from XUL\n");

  // 5 = nsMsgSaveAsTemplate
  // RICHIE: We should really have a way of using constants and not
  // hardcoded numbers for the first argument
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
			if (msgCompFields.GetFcc2() == destUri)
			{
				msgCompFields.SetFcc2("nocopy://");
				dump("FCC2: none\n");
			}
			else
			{
				msgCompFields.SetFcc2(destUri);
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
			msgCompFields.SetPriority(target.getAttribute('id'));
	}
}

function ReturnReceiptMenuSelect()
{
	if (msgCompose)
	{
		var msgCompFields = msgCompose.compFields;
		if (msgCompFields)
		{
			if (msgCompFields.GetReturnReceipt())
			{
				dump("Set Return Receipt to FALSE\n");
				msgCompFields.SetReturnReceipt(false);
			}
			else
			{
				dump("Set Return Receipt to TRUE\n");
				msgCompFields.SetReturnReceipt(true);
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
	
	var toAddress = msgCompFields.GetTo();
	var ccAddress = msgCompFields.GetCc();
	var bccAddress = msgCompFields.GetBcc();
	
	dump("toAddress: " + toAddress + "\n");
	window.openDialog("chrome://messenger/content/addressbook/abSelectAddressesDialog.xul",
					  "",
					  "chrome,resizable,modal",
					  {composeWindow:top.window,
					   msgCompFields:msgCompFields,
					   toAddress:toAddress,
					   ccAddress:ccAddress,
					   bccAddress:bccAddress});
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
			var	result = serverSupports.GetElementAt(0).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
		//dump ("The account name is = "+result.prettyName+ "\n");
		var accountName = " - "+result.prettyName;

        var item=document.createElement('menuitem');
        item.setAttribute('value', identity.identityName);
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

function AdjustFocus()
{
    var element = document.getElementById("msgRecipient#" + awGetNumberOfRecipients());
	if (element.value == "")
	{
		dump("set focus on the recipient\n");
		awSetFocus(awGetNumberOfRecipients(), element);
	}
	else
	{
	    element = document.getElementById("msgSubject");
	    if (element.value == "")
	    {
    		dump("set focus on the subject\n");
    		element.focus();
	    }
	    else
    	{
    		dump("set focus on the body\n");
    		editorShell.contentWindow.focus();
    	}
    }
}

function SetComposeWindowTitle(event) 
{
	/* dump("event = " + event + "\n"); */

	/* only set the title when they hit return (or tab?) if 
	  mail.update_compose_title_as_you_type == false
	 */
	if ((event != 13) && (update_compose_title_as_you_type == false)) {
		return;
	}

	newTitle = document.getElementById('msgSubject').value;

	/* dump("newTitle = " + newTitle + "\n"); */

	if (newTitle == "" ) {
		newTitle = Bundle.GetStringFromName("defaultSubject");
	}

	newTitle += GetCharsetUIString();

	window.title = Bundle.GetStringFromName("windowTitlePrefix") + " " + newTitle;
}

// Check for changes to document and allow saving before closing
// This is hooked up to the OS's window close widget (e.g., "X" for Windows)
function ComposeCanClose()
{
	// Returns FALSE only if user cancels save action
	if (contentChanged || msgCompose.bodyModified)
	{
		if (commonDialogsService)
		{
            var result = {value:0};
			commonDialogsService.UniversalDialog(
				window,
				null,
				Bundle.GetStringFromName("saveDlogTitle"),
				Bundle.GetStringFromName("saveDlogMessage"),
				null,
				Bundle.GetStringFromName("saveDlogSaveBtn"),
				Bundle.GetStringFromName("saveDlogCancelBtn"),
				Bundle.GetStringFromName("saveDlogDontSaveBtn"),
				null,
				null,
				null,
				{value:0},
				{value:0},
				"chrome://global/skin/question-icon.gif",
				{value:"false"},
				3,
				0,
				0,
				result
				);
			if (result)
			{
				switch (result.value)
				{
					case 0: //Save
                        if (LastToClose())
                            NotifyQuitApplication();
						SaveAsDraft();
						break;
					case 1:	//Cancel
						return false;
					case 2:	//Don't Save
						break;	
				}	
			}
		}
				
		msgCompose.bodyModified = false;
		contentChanged = false;
	}

	return true;
}

function CloseWindow()
{
	if (msgCompose)
		msgCompose.CloseWindow();
}

function AttachFile()
{
	dump("AttachFile()\n");
	currentAttachment = "";
	//Get file using nsIFilePicker and convert to URL
    try {
			var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
			fp.init(window, Bundle.GetStringFromName("chooseFileToAttach"), nsIFilePicker.modeOpen);
			fp.appendFilters(nsIFilePicker.filterAll);
			if (fp.show() == nsIFilePicker.returnOK) {
			currentAttachment = fp.fileURL.spec;
			dump("nsIFilePicker - "+currentAttachment+"\n");
			}
    } 	
	catch (ex) {
		dump("failed to get the local file to attach\n");
	}
	
	AddAttachment(currentAttachment);
}

function AddAttachment(attachment)
{
	if (attachment && (attachment != ""))
	{
		var bucketBody = document.getElementById("bucketBody");
		var item = document.createElement("treeitem");
		var row = document.createElement("treerow");
		var cell = document.createElement("treecell");
		
		if (msgCompose)
			prettyName = msgCompose.AttachmentPrettyName(attachment);
		cell.setAttribute("value", prettyName);				//use for display only
		cell.setAttribute("attachment", attachment);		//full url stored here
		cell.setAttribute("tooltip", "aTooltip");
		cell.setAttribute("tooltiptext", unescape(attachment));
		row.appendChild(cell);
		item.appendChild(row);
		bucketBody.appendChild(item);
	}    
}

function AttachPage()
{
    if (commonDialogsService)
    {
        var result = {value:0};
        if (commonDialogsService.Prompt(
        	window,
        	Bundle.GetStringFromName("attachPageDlogTitle"),
        	Bundle.GetStringFromName("attachPageDlogMessage"),
        	null,
        	result))
        {
			AddAttachment(result.value);
        }
    }
}

function GenerateAttachmentsString()
{
	var attachments = "";
	var body = document.getElementById('bucketBody');
	var item, row, cell, text, colon;

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
					text = cell.getAttribute("attachment");
					if (text.length)
					{
						if (attachments == "")
							attachments = text;
						else
							attachments = attachments + "," + text;
					}
				}
			}
		}
	}

	return attachments;
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

function DetermineHTMLAction()
{
    if (! msgCompose.composeHTML)
    {
        try {
            msgCompose.CheckAndPopulateRecipients(true, false);
        } catch(ex) {}
        return msgCompSendFormat.PlainText;
    }

    if (sendFormat == msgCompSendFormat.AskUser)
    {
        //Well, before we ask, see if we can figure out what to do for ourselves
        
        var noHtmlRecipients;
        var noHtmlnewsgroups;

        //Check the address book for the HTML property for each recipient
        try {
            noHtmlRecipients = msgCompose.CheckAndPopulateRecipients(true, true);
        } catch(ex)
        {
            var msgCompFields = msgCompose.compFields;
            noHtmlRecipients = msgCompFields.GetTo() + "," + msgCompFields.GetCc() + "," + msgCompFields.GetBcc();
        }
        dump("DetermineHTMLAction: noHtmlRecipients are " + noHtmlRecipients + "\n");

        //Check newsgroups now...
        try {
            noHtmlnewsgroups = msgCompose.GetNoHtmlNewsgroups(null);
        } catch(ex)
        {
           noHtmlnewsgroups = msgCompose.compFields.GetNewsgroups();
        }
        
        if (noHtmlRecipients != "" || noHtmlnewsgroups != "")
        {
            try {
                var bodyContainsHTML = msgCompose.BodyContainsHTMLTag(window.editorShell.contentWindow.document.childNodes[1]);
                if (! bodyContainsHTML)
                  return msgCompSendFormat.PlainText;
            } catch(ex) {}
            
            if (noHtmlnewsgroups == "")
            {
                //See if a preference has been set to tell us what to do. Note that we do not honor that
                //preference for newsgroups. Only for e-mail addresses.
                action = prefs.GetIntPref("mail.default_html_action");
                switch (action)
                {
                    case msgCompSendFormat.PlainText    :
                    case msgCompSendFormat.HTML         :
                    case msgCompSendFormat.Both         :
                        return action;
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
			  msgCompose.CheckAndPopulateRecipients(true, false);
		  } catch(ex) {}
	  }

    return sendFormat;
}

function LoadIdentity(startup)
{
    var identityElement = document.getElementById("msgIdentity");
    
    prevIdentity = currentIdentity;
    
    if (identityElement) {
        var item = identityElement.selectedItem;
        idKey = item.getAttribute('id');
        currentIdentity = accountManager.getIdentity(idKey);

        if (!startup && prevIdentity && idKey != prevIdentity.key)
        {
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
          
          var newReplyTo = currentIdentity.replyTo;
          var newBcc = "";
          if (currentIdentity.bccSelf)
            newBcc += currentIdentity.email;
          if (currentIdentity.bccOthers)
          {
            if (newBcc != "")
              newBcc += ","
            newBcc += currentIdentity.bccList; 
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
            if (newReplyTo != "")
              awAddRecipients(msgCompFields, "addr_bcc", newBcc);
          }          
          
          if (needToCleanUp)
            awCleanupRows();

          try {
            msgCompose.SetSignature(currentIdentity);
          } catch (ex) { dump("### Cannot set the signature: " + ex + "\n");}
        }
        
        //Setup autocomplete session, we can doit from here as it's use as a service
        var session = Components.classes["component://netscape/autocompleteSession&type=addrbook"].getService(Components.interfaces.nsIAbAutoCompleteSession);
        if (session)
        {
            var emailAddr = currentIdentity.email;
            var start = emailAddr.lastIndexOf("@");
            session.defaultDomain = emailAddr.slice(start + 1, emailAddr.length);
        }
    }   
}
