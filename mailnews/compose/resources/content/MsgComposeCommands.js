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

var msgComposeService = Components.classes["component://netscape/messengercompose"].getService();
msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);
var msgCompose = null;
var MAX_RECIPIENTS = 0;
var numAttachments = 0;
var currentAttachment = null;

var other_header = "";
var update_compose_title_as_you_type = true;
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

var editorDocumentListener = {
	NotifyDocumentCreated: function() {
		if (document.getElementById("msgRecipient#1").value == "")
		{
			dump("set focus on the recipient\n");
			document.getElementById("msgRecipient#1").focus();
		}
		else
		{
			dump("set focus on the body\n");
			contentWindow.focus();
		}
		SetComposeWindowTitle(13);
	}
};

function GetArgs()
{
	var args = new Object();
	var originalData = document.getElementById("args").getAttribute("value");
	var data = "";
	var separator = String.fromCharCode(1);

	var quoteChar = "";
	for (var i = 0; i < originalData.length; i ++)
	{
		var aChar = originalData.charAt(i)
		var aCharCode = originalData.charCodeAt(i)
		if (aChar == quoteChar)
		{
			quoteChar = "";
		}
		else if (aCharCode == 39 || aCharCode == 34) //quote or double quote
		{
			if (quoteChar == "")
				quoteChar = aChar;
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

function ComposeStartup()
{
	dump("Compose: ComposeStartup\n");

	// Get arguments
	var args = GetArgs();

    // fill in Identity combobox
    var identitySelect = document.getElementById("msgIdentity");
    if (identitySelect) {
        fillIdentitySelect(identitySelect);
    }
    
    // fill in Recipient type combobox
    FillRecipientTypeCombobox();
    
	if (msgComposeService)
	{
		msgCompose = msgComposeService.InitCompose(window, args.originalMsg, args.type, args.format, args.fieldsAddr);
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

			SetupToolbarElements(); //defined into EditorCommands.js
            contentWindow = window.content;

			// setEditorType MUST be call before setContentWindow
			if (msgCompose.composeHTML)
			{
				window.editorShell.SetEditorType("html");
				dump("editor initialized in HTML mode\n");
			}
			else
			{
			    //Remove HTML toolbar as we are editing in plain text mode
			    document.getElementById("FormatToolbar").setAttribute("hidden", true);

				window.editorShell.SetEditorType("text");
				try
				{
					window.editorShell.wrapColumn = msgCompose.wrapLength;
				}
				catch (e)
				{
					dump("### window.editorShell.wrapColumn exception text: " + e + " - failed\n");
				}
				dump("editor initialized in PLAIN TEXT mode\n");
			}

			window.editorShell.SetContentWindow(contentWindow);
			window.editorShell.SetWebShellWindow(window);
			window.editorShell.SetToolbarWindow(window);
			window.editorShell.RegisterDocumentStateListener(editorDocumentListener);

			// Now that we have an Editor AppCore, we can finish to initialize the Compose AppCore
			msgCompose.editor = window.editorShell;			
			
	    	var msgCompFields = msgCompose.compFields;
	    	if (msgCompFields)
	    	{
	    		if (args.body) //We need to set the body before calling LoadFields();
	    			msgCompFields.SetBody(args.body);

				//Now we are ready to load all the fields (to, cc, subject, body, etc...)  depending of the type of the message
				msgCompose.LoadFields();

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
			
				CompFields2Recipients(msgCompFields);
				var subjectValue = msgCompFields.GetSubject();
				if (subjectValue != "") {
					document.getElementById("msgSubject").value = subjectValue;
				}
			}
			
			document.getElementById("msgRecipient#1").focus();
		}
	}
}

function ComposeLoad()
{
	dump("\nComposeLoad from XUL\n");
	var selectNode = document.getElementById('msgRecipientType#1');

	if (other_header != "") {
		var opt = new Option(other_header + ":", "addr_other");
		selectNode.add(opt, null); 
	}
}

function ComposeUnload(calledFromExit)
{
	dump("\nComposeUnload from XUL\n");

	if (msgCompose && msgComposeService)
		msgComposeService.DisposeCompose(msgCompose, false);
	//...and what's about the editor appcore, how can we release it?
}

function SetDocumentCharacterSet(aCharset)
{
	dump("SetDocumentCharacterSet Callback!\n");
	dump(aCharset + "\n");

	if (msgCompose)
		msgCompose.SetDocumentCharset(aCharset);
	else
		dump("Compose has not been created!\n");
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
			msgCompFields.SetSubject(document.getElementById("msgSubject").value);
			dump("attachments = " + GenerateAttachmentsString() + "\n");
			try {
				msgCompFields.SetAttachments(GenerateAttachmentsString());
			}
			catch (ex) {
				dump("failed to SetAttachments\n");
			}
		
			try {
				msgCompose.SendMsg(msgType, getCurrentIdentity(), null);
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
	GenericSendMessage(0);
}

function SendMessageLater()
{
	dump("SendMessageLater from XUL\n");
  // 1 = nsMsgQueueForLater
  // RICHIE: We should really have a way of using constants and not
  // hardcoded numbers for the first argument
	GenericSendMessage(1);
}

function SaveAsDraft()
{
	dump("SaveAsDraft from XUL\n");

  // 4 = nsMsgSaveAsDraft
  // RICHIE: We should really have a way of using constants and not
  // hardcoded numbers for the first argument
  GenericSendMessage(4);
}

function SaveAsTemplate()
{
	dump("SaveAsDraft from XUL\n");

  // 5 = nsMsgSaveAsTemplate
  // RICHIE: We should really have a way of using constants and not
  // hardcoded numbers for the first argument
  GenericSendMessage(5);
}


function MessageFcc(menuItem)
{
	// Get the id for the folder we're FCC into
	destUri = menuItem.getAttribute('id');
	if (msgCompose)
	{
		var msgCompFields = msgCompose.compFields;
		if (msgCompFields)
		{
			if (msgCompFields.GetFcc() == destUri)
			{
				msgCompFields.SetFcc("nocopy://");
				dump("FCC: none\n");
			}
			else
			{
				msgCompFields.SetFcc(destUri);
				dump("FCC: " + destUri + "\n");
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
//	dump(target.getAttribute('id') + "\n");
}

function SelectAddress() 
{
	var msgCompFields = msgCompose.compFields;
	
	Recipients2CompFields(msgCompFields);
	
	var toAddress = msgCompFields.GetTo();
	var ccAddress = msgCompFields.GetCc();
	var bccAddress = msgCompFields.GetBcc();
	
	dump("toAddress: " + toAddress + "\n");
	window.openDialog("chrome://addressbook/content/abSelectAddressesDialog.xul",
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
      result[i] = supportsArray.GetElementAt(i).QueryInterface(iid);
    }
    return result;
}

function GetIdentities()
{
    var msgService = Components.classes['component://netscape/messenger/services/session'].getService(Components.interfaces.nsIMsgMailSession);

    var accountManager = msgService.accountManager;

    var idSupports = accountManager.allIdentities;
    var identities = queryISupportsArray(idSupports,
                                         Components.interfaces.nsIMsgIdentity);

    return identities;
}

function fillIdentitySelect(selectElement)
{
    var identities = GetIdentities();

    for (var i=0; i<identities.length; i++)
    {
		var identity = identities[i];
		var opt = new Option(identity.identityName, identity.key);

		selectElement.add(opt, null);
    }
}

function getCurrentIdentity()
{
    var msgService = Components.classes['component://netscape/messenger/services/session'].getService(Components.interfaces.nsIMsgMailSession);
    var accountManager = msgService.accountManager;

    // fill in Identity combobox
    var identitySelect = document.getElementById("msgIdentity");
    var identityKey = identitySelect.value;
    dump("Looking for identity " + identityKey + "\n");
    var identity = accountManager.getIdentity(identityKey);
    
    return identity;
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
		/* i18N todo:  this should not be hard coded */
		newTitle = '(no subject)';
	}
	/* i18N todo:  this should not be hard coded, either */
	window.title = "Compose: "+ newTitle;
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
		currentAttachment = filePicker.chooseFile("Enter file to attach");
	}
	catch (ex) {
		dump("failed to get the local file to attach\n");
	}
	
	AddAttachment(currentAttachment);
}

function AddAttachment(attachment)
{
	if (attachment && (attachment != "")) {
		/* dump("attachment = " + attachment + "\n"); */
		selectNode = document.getElementById('attachments');
		numAttachments = numAttachments + 1;
		key = "attachment" + numAttachments;
                var opt = new Option(attachment, key);
		selectNode.add(opt, null);
        }    
}

function AttachPage()
{
	window.openDialog("chrome://messengercompose/content/MsgAttachPage.xul", "attachPageDialog", "chrome", {addattachmentfunction:AddAttachment});
}

function GenerateAttachmentsString()
{
	dump("GenerateAttachmentsString()\n");
	attachments = "";

	selectNode = document.getElementById('attachments');
	if (selectNode == null) return attachments;
	options = selectNode.options;
	if (options == null) return attachments;
	if (options.length == 0) return attachments;

	attachments = options[0].text;

	for (i=1;i<options.length;i++) {
		attachments = attachments + "," + options[i].text;
	}

	return attachments;
}

function RemoveLastAttachment()
{
	dump("RemoveLastAttachment()\n");
	selectNode = document.getElementById('attachments');
	i = selectNode.options.length;
	if (i > 0) {
		selectNode.remove(i - 1);
	}
}

function AttachVCard()
{
	dump("AttachVCard()\n");
}
