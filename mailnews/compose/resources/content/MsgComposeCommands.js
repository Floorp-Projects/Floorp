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

function GetArgs()
{
	var args = new Object();
	var data = document.getElementById("args").getAttribute("value");
	var pairs = data.split(",");
	//dump("Compose: argument: {" + data + "}\n");

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
	}
	return args;
}

function ComposeStartup()
{
	dump("Compose: ComposeStartup\n");

	// Get arguments
	var args = GetArgs();
	dump("[type=" + args.type + "]\n");
	dump("[format=" + args.format + "]\n");
	dump("[originalMsg=" + args.originalMsg + "]\n");

    // fill in Identity combobox
    var identitySelect = document.getElementById("msgIdentity");
    if (identitySelect) {
        fillIdentitySelect(identitySelect);
    }
    
    // fill in Recipient type combobox
    FillRecipientTypeCombobox();

	if (msgComposeService)
	{
		msgCompose = msgComposeService.InitCompose(window, args.originalMsg, args.type, args.format);
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
			contentWindow = window.frames[0]; //Patch to make EditorCommands.js happy...

			// setEditorType MUST be call before setContentWindow
			if (msgCompose.composeHTML)
			{
				window.editorShell.SetEditorType("html");
				dump("editor initialized in HTML mode\n");
			}
			else
			{
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

			window.editorShell.SetContentWindow(window.frames[0]);
			window.editorShell.SetWebShellWindow(window);
			window.editorShell.SetToolbarWindow(window);

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
				if (subjectValue != "")
					document.getElementById("msgSubject").value = subjectValue;
			}
			
			document.getElementById("msgRecipient#1").focus();
		}
	}
}


function ComposeUnload(calledFromExit)
{
	dump("\nComposeUnload from XUL\n");

	if (msgCompose && msgComposeService)
		msgComposeService.DisposeCompose(msgCompose, false);
	//...and what's about the editor appcore, how can we release it?
}

function ComposeExit()
{
	dump("\nComposeExit from XUL\n");

	//editor appcore knows how to shutdown the application, just use it...
	window.editorShell.Exit();
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
		
			msgCompose.SendMsg(msgType, getCurrentIdentity(), null);
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

function SelectAddress() 
{
	var msgCompFields = msgCompose.compFields;
	
	Recipients2CompFields(msgCompFields);
	
	var toAddress = msgCompFields.GetTo();
	var ccAddress = msgCompFields.GetCc();
	var bccAddress = msgCompFields.GetBcc();
	
	dump("toAddress: " + toAddress + "\n");
	var dialog = window.openDialog("chrome://addressbook/content/selectaddress.xul",
								   "selectAddress",
								   "chrome",
								   {composeWindow:top.window,
								    msgCompFields:msgCompFields,
								    toAddress:toAddress,
								    ccAddress:ccAddress,
								    bccAddress:bccAddress});
	
	return dialog;
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
    var identity = accountManager.GetIdentityByKey(identityKey);
    
    return identity;
}

function FillRecipientTypeCombobox()
{
	originalCombo = document.getElementById("msgRecipientType#1");
	if (originalCombo)
	{
		MAX_RECIPIENTS = 2;
	    while ((combo = document.getElementById("msgRecipientType#" + MAX_RECIPIENTS)))
	    {
	    	for (var j = 0; j < originalCombo.length; j ++)
				combo.add(new Option(originalCombo.options[j].text,
									 originalCombo.options[j].value), null);
			MAX_RECIPIENTS++;
		}
		MAX_RECIPIENTS--;
	}
}

function Recipients2CompFields(msgCompFields)
{
	if (msgCompFields)
	{
		var i = 1;
		var addrTo = "";
		var addrCc = "";
		var addrBcc = "";
		var addrNg = "";
		var to_Sep = "";
		var cc_Sep = "";
		var bcc_Sep = "";
		var ng_Sep = "";

	    while ((inputField = document.getElementById("msgRecipient#" + i)))
	    {
	    	fieldValue = inputField.value;
	    	if (fieldValue != "")
	    	{
	    		switch (document.getElementById("msgRecipientType#" + i).value)
	    		{
	    			case "addr_to"			: addrTo += to_Sep + fieldValue; to_Sep = ",";		break;
	    			case "addr_cc"			: addrCc += cc_Sep + fieldValue; cc_Sep = ",";		break;
	    			case "addr_bcc"			: addrBcc += bcc_Sep + fieldValue; bcc_Sep = ",";	break;
	    			case "addr_newsgroups"	: addrNg += ng_Sep + fieldValue; ng_Sep = ",";		break;
	    		}
	    	}
	    	i ++;
	    }
    	msgCompFields.SetTo(addrTo);
    	msgCompFields.SetCc(addrCc);
    	msgCompFields.SetBcc(addrBcc);
    	msgCompFields.SetNewsgroups(addrNg);
	}
	else
		dump("Message Compose Error: msgCompFields is null (ExtractRecipients)");
}

function CompFields2Recipients(msgCompFields)
{
	if (msgCompFields)
	{
		var i = 1;
		var fieldValue = msgCompFields.GetTo();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_to";
			i ++;
		}

		fieldValue = msgCompFields.GetCc();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_cc";
			i ++;
		}

		fieldValue = msgCompFields.GetBcc();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_bcc";
			i ++;
		}		

		fieldValue = msgCompFields.GetNewsgroups();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_newsgroups";
			i ++;
		}
		
		for (; i <= MAX_RECIPIENTS; i++) 
		{ 
			document.getElementById("msgRecipient#" + i).value = ""; 
			document.getElementById("msgRecipientType#" + i).value = "addrTo"; 
		}
	}
}
