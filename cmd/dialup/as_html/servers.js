/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
<!--  to hide script contents from old browsers



function go()
{
	if (parent.parent.globals.document.vars.editMode.value == "yes")
		return true;
	else
		return(checkData());
}



function checkData()
{
	return(true);
}



function updateMailProtocols(theObject)
{
	var popServer="";
	var imapServer="";

	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");
	
	var providerFilename = parent.parent.globals.document.vars.providerFilename.value;
	if (providerFilename != "")	{
		popServer = parent.parent.globals.document.setupPlugin.GetNameValuePair(providerFilename, "Services", "POP_Server");
		imapServer = parent.parent.globals.document.setupPlugin.GetNameValuePair(providerFilename, "Services", "IMAP_Server");
		}

	if (theObject.name == "IMAP")	{
		document.forms[0].POP.checked = false;
		document.forms[0].IMAP.checked = true;
		if (document.forms[0].Mail_Server.value == popServer || document.forms[0].Mail_Server.value == "")	{
			document.forms[0].Mail_Server.value = imapServer;
			}
		}
	else	{
		document.forms[0].POP.checked = true;
		document.forms[0].IMAP.checked = false;
		if (document.forms[0].Mail_Server.value == imapServer || document.forms[0].Mail_Server.value == "")	{
			document.forms[0].Mail_Server.value = popServer;
			}
		}
}



function loadData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
	if (((document.forms[0].SMTP == "undefined") || (document.forms[0].SMTP == "[object InputArray]")) ||
		((document.forms[0].Mail_Server == "undefined") || (document.forms[0].Mail_Server == "[object InputArray]")) ||
		((document.forms[0].IMAP == "undefined") || (document.forms[0].IMAP == "[object InputArray]")) ||
		((document.forms[0].NNTP == "undefined") || (document.forms[0].NNTP == "[object InputArray]")) ||
		((document.forms[0].POP == "undefined") || (document.forms[0].POP == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	document.forms[0].SMTP.value = parent.parent.globals.document.vars.SMTP.value;
	document.forms[0].Mail_Server.value = parent.parent.globals.document.vars.mailServer.value;

	var providerFilename = parent.parent.globals.document.vars.providerFilename.value;

	var mailProtocol = parent.parent.globals.document.vars.mailProtocol.value;
	mailProtocol = mailProtocol.toUpperCase();
	if (mailProtocol == "IMAP")	{
		document.forms[0].POP.checked = false;
		document.forms[0].IMAP.checked = true;

		if (providerFilename != "" && document.forms[0].Mail_Server.value == "")	{
			document.forms[0].Mail_Server.value = parent.parent.globals.document.setupPlugin.GetNameValuePair(providerFilename, "Services", "IMAP_Server");
			}

		}
	else	{
		document.forms[0].POP.checked = true;
		document.forms[0].IMAP.checked = false;

		if (providerFilename != "" && document.forms[0].Mail_Server.value == "")	{
			document.forms[0].Mail_Server.value = parent.parent.globals.document.setupPlugin.GetNameValuePair(providerFilename, "Services", "POP_Server");
			}

		}

	document.forms[0].NNTP.value = parent.parent.globals.document.vars.NNTP.value;
	parent.parent.globals.setFocus(document.forms[0].SMTP);

/*
	var theFile = parent.parent.globals.getAcctSetupFilename(self);

	var theData = parent.parent.globals.document.setupPlugin.GetNameValuePair(theFile, "Existing Acct Mode", "AskIMAP");
	if (theData != null)	{
		theData = theData.toLowerCase();
		if (theData == "yes")	{

			// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
			if ((document.forms[0].IMAP == "undefined") || (document.forms[0].IMAP == "[object InputArray]")) {
				parent.controls.reloadDocument();
				return;
				}

			document.forms[0].IMAP.value = parent.parent.globals.document.vars.IMAP.value;
			}
		}
	theData = parent.parent.globals.document.setupPlugin.GetNameValuePair(theFile, "Existing Acct Mode", "AskLDAP");
	if (theData != null)	{
		theData = theData.toLowerCase();
		if (theData == "yes")	{
			// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
			if ((document.forms[0].LDAP == "undefined") || (document.forms[0].LDAP == "[object InputArray]")) {
				parent.controls.reloadDocument();
				return;
				}
			document.forms[0].LDAP.value = parent.parent.globals.document.vars.LDAP.value;
			}
		}
*/
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	// make sure all form element are valid objects, otherwise just skip & return!
	if (((document.forms[0].SMTP == "undefined") || (document.forms[0].SMTP == "[object InputArray]")) ||
		((document.forms[0].Mail_Server == "undefined") || (document.forms[0].Mail_Server == "[object InputArray]")) ||
		((document.forms[0].IMAP == "undefined") || (document.forms[0].IMAP == "[object InputArray]")) ||
		((document.forms[0].NNTP == "undefined") || (document.forms[0].NNTP == "[object InputArray]")) ||
		((document.forms[0].POP == "undefined") || (document.forms[0].POP == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	parent.parent.globals.document.vars.SMTP.value = document.forms[0].SMTP.value;
//	parent.parent.globals.document.vars.POP.value = document.forms[0].POP.value;
	parent.parent.globals.document.vars.NNTP.value = document.forms[0].NNTP.value;

	if (document.forms[0].IMAP.checked == true)	{
		parent.parent.globals.document.vars.mailProtocol.value = document.forms[0].IMAP.value;
		}
	else	{
		parent.parent.globals.document.vars.mailProtocol.value = document.forms[0].POP.value;
		}
	parent.parent.globals.document.vars.mailServer.value = document.forms[0].Mail_Server.value;

/*
	var theFile = parent.parent.globals.getAcctSetupFilename(self);
	var theData;

	theData = parent.parent.globals.document.setupPlugin.GetNameValuePair(theFile, "Existing Acct Mode", "AskIMAP");
	if (theData != null)	{
		theData = theData.toLowerCase();
		if (theData == "yes")	{
			parent.parent.globals.document.vars.IMAP.value=document.forms[0].IMAP.value;
			}
		}
	theData = parent.parent.globals.document.setupPlugin.GetNameValuePair(theFile, "Existing Acct Mode", "AskLDAP");
	if (theData != null)	{
		theData = theData.toLowerCase();
		if (theData == "yes")	{
			parent.parent.globals.document.vars.LDAP.value=document.forms[0].LDAP.value;
			}
		}
*/
}



/*
function IMAPOptions()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var theFile = parent.parent.globals.getAcctSetupFilename(self);

	var theData = parent.parent.globals.document.setupPlugin.GetNameValuePair(theFile, "Existing Acct Mode", "AskIMAP");
	if (theData != null)	{
		theData = theData.toLowerCase();
		if (theData == "yes")	{
		    document.writeln("<TR><TD COLSPAN='3'><spacer type=vertical size=0></TD></TR>");
		    document.writeln("<TR><TD VALIGN='BOTTOM' ALIGN='RIGHT'>");
		    document.writeln("<FONT FACE='Humnst777 LT,Helvetica,Arial' POINT-SIZE='10' COLOR='#000000'><B>");
			document.writeln("IMAP Mail Server:");
			document.writeln("</B><spacer type=vertical size=2></FONT></TD><TD ALIGN='LEFT' VALIGN='BOTTOM'>");
			document.writeln("<INPUT NAME='IMAP' TYPE='text' SIZE=32 MAXLENGTH=40></TD><TD></TD></TR>");

			}
		}
}



function LDAPOptions()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var theFile = parent.parent.globals.getAcctSetupFilename(self);

	var theData = parent.parent.globals.document.setupPlugin.GetNameValuePair(theFile, "Existing Acct Mode", "AskLDAP");
	if (theData != null)	{
		theData = theData.toLowerCase();
		if (theData == "yes")	{
			document.writeln("<TD ALIGN=right>LDAP server(s):</TD>");
			document.writeln("<TD><TEXTAREA ROWS=3 COLS=40 NAME='LDAP' TYPE='text'></TEXTAREA></TD>");
			}
		}
}
*/



// end hiding contents from old browsers  -->
