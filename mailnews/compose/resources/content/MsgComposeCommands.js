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

var messengerMigratorProgID   = "component://netscape/messenger/migrator";

var msgComposeService = Components.classes["component://netscape/messengercompose"].getService();
msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

var commonDialogsService = Components.classes["component://netscape/appshell/commonDialogs"].getService();
commonDialogsService = commonDialogsService.QueryInterface(Components.interfaces.nsICommonDialogs);

var msgCompose = null;
var MAX_RECIPIENTS = 0;
var currentAttachment = null;
var documentLoaded = false;
var contentChanged = false;

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
//		dump("\n RECEIVE NotifyComposeFieldsReady\n\n");
		documentLoaded = true;
		msgCompose.UnregisterStateListener(stateListener);
	}
};

// i18n globals
var currentMailSendCharset = null;
var g_send_default_charset = null;
var g_charsetTitle = null;

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
		window.updateCommands("create");
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

    var identity;
    if (args.preselectid)
        identity = getIdentityForKey(args.preselectid);
    else
    {
        // no preselect, so use the default account
        var identities = accountManager.defaultAccount.identities;
        
    	identity = identities.QueryElementAt(0, Components.interfaces.nsIMsgIdentity);
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

            contentWindow = window.content;

			// setEditorType MUST be call before setContentWindow
			if (msgCompose.composeHTML)
			{
				window.editorShell.SetEditorType("htmlmail");
				dump("editor initialized in HTML mode\n");
			}
			else
			{
			    //Remove HTML toolbar and format menu as we are editing in plain text mode
			    document.getElementById("FormatToolbar").setAttribute("hidden", true);
			    document.getElementById("formatMenu").setAttribute("hidden", true);

				window.editorShell.SetEditorType("textmail");
				dump("editor initialized in PLAIN TEXT mode\n");
			}

			window.editorShell.webShellWindow = window;
			window.editorShell.contentWindow = contentWindow;

			// set up JS-implemented commands
			SetupControllerCommands();

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

			msgCompose.RegisterStateListener(stateListener);

			// call updateCommands to disable while we're loading the page
			window.updateCommands("create");
			
			WaitFinishLoadingDocument(args.type);
		}
	}
}

function MsgAccountWizard()
{
    var result = {refresh: false};

    window.openDialog("chrome://messenger/content/AccountWizard.xul",
                      "AccountWizard", "chrome,modal", result);

    if (result.refresh)	{
	dump("anything to refresh here?\n");
    }
}

function ComposeLoad()
{
	dump("\nComposeLoad from XUL\n");

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
	
	if (msgCompose && msgComposeService)
		msgComposeService.DisposeCompose(msgCompose, false);
	//...and what's about the editor appcore, how can we release it?
}

function SetDocumentCharacterSet(event)
{
  var aCharset = event.target.getAttribute('id');

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
  var send_default_charset = prefs.CopyCharPref("mailnews.send_default_charset");
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
      g_send_default_charset = prefs.CopyCharPref("mailnews.send_default_charset");
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
				msgCompose.SendMsg(msgType, getCurrentIdentity(), null);
				contentChanged = false;
				msgCompose.bodyModified = false;
			}
			catch (ex) {
				dump("failed to SendMsg\n");
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

function SaveAsDraft()
{
	dump("SaveAsDraft from XUL\n");

  // 4 = nsMsgSaveAsDraft
  // RICHIE: We should really have a way of using constants and not
  // hardcoded numbers for the first argument
  GenericSendMessage(msgCompDeliverMode.SaveAsDraft);
}

function SaveAsTemplate()
{
	dump("SaveAsTemplate from XUL\n");

  // 5 = nsMsgSaveAsTemplate
  // RICHIE: We should really have a way of using constants and not
  // hardcoded numbers for the first argument
  GenericSendMessage(msgCompDeliverMode.SaveAsTemplate);
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

function UUEncodeMenuSelect()
{
	if (msgCompose)
	{
		var msgCompFields = msgCompose.compFields;
		if (msgCompFields)
		{
			if (msgCompFields.GetUUEncodeAttachments())
			{
				dump("Set Return UUEncodeAttachments to FALSE\n");
				msgCompFields.SetUUEncodeAttachments(false);
			}
			else
			{
				dump("Set Return UUEncodeAttachments to TRUE\n");
				msgCompFields.SetUUEncodeAttachments(true);
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
    		    default: break;		
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

        var item=document.createElement('menuitem');
        item.setAttribute('value', identity.identityName);
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
    		contentWindow.focus();
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

	// Get a local file, converted into URL format
	try {
		var filePicker = Components.classes["component://netscape/filespecwithui"].createInstance();
    filePicker = filePicker.QueryInterface(Components.interfaces.nsIFileSpecWithUI);
		currentAttachment = filePicker.chooseFile(Bundle.GetStringFromName("chooseFileToAttach"));
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
        return msgCompSendFormat.PlainText;

    if (sendFormat == msgCompSendFormat.AskUser)
    {
        //Well, before we ask, see if we can figure out what to do for ourselves
        
        var noHtmlRecipients;
        var noHtmlnewsgroups;

        //Check the address book for the HTML property for each recipient
        try {
            noHtmlRecipients = msgCompose.GetNoHtmlRecipients(null);
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
            
            //Do we really need to send in HTML?
            //FIX ME: need to ask editor is the body containg any formatting or non plaint text elements.
            
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

    return sendFormat;
}

function LoadIdentity(startup)
{
    var identityElement = document.getElementById("msgIdentity");
    
    if (identityElement) {
        var item = identityElement.selectedItem;
        idKey = item.getAttribute('id');
        var identity = accountManager.getIdentity(idKey);
        
        //Setup autocomplete session, we can doit from here as it's use as a service
        var session = Components.classes["component://netscape/autocompleteSession&type=addrbook"].getService(Components.interfaces.nsIAbAutoCompleteSession);
        if (session)
        {
            var emailAddr = identity.email;
            var start = emailAddr.lastIndexOf("@");
            session.defaultDomain = emailAddr.slice(start + 1, emailAddr.length);
        }
    }   
}
