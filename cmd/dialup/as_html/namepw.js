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
	if (document.forms[0].accountName.value == "")	{
		alert("You must enter a login name.");
		parent.parent.globals.setFocus(document.forms[0].accountName);
		return(false);
		}
	if (document.forms[0].accountPassword.value != document.forms[0].accountPasswordCheck.value)	{
		if (document.forms[0].accountPassword.value == "")	{
			parent.parent.globals.setFocus(document.forms[0].accountPassword);
			}
		else	{
			parent.parent.globals.setFocus(document.forms[0].accountPasswordCheck);
			}
		alert("The password you entered in 'Type Password Again' does not match the password you entered in 'Password'. Please re-enter your password.");
		return(false);
		}
	return true;
}



function loadData()
{
	// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
	if (((document.forms[0].accountName == "undefined") || (document.forms[0].accountName == "[object InputArray]")) ||
		((document.forms[0].accountPassword == "undefined") || (document.forms[0].accountPassword == "[object InputArray]")) ||
		((document.forms[0].accountPasswordCheck == "undefined") || (document.forms[0].accountPasswordCheck == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	document.forms[0].accountName.value = parent.parent.globals.document.vars.accountName.value;
	document.forms[0].accountPassword.value = parent.parent.globals.document.vars.accountPassword.value;
	document.forms[0].accountPasswordCheck.value = parent.parent.globals.document.vars.accountPasswordCheck.value;
	parent.parent.globals.setFocus(document.forms[0].accountName);

	var theFile = parent.parent.globals.getAcctSetupFilename(self);
	var ttyFlag = parent.parent.globals.GetNameValuePair(theFile,"Existing Acct Mode","AskTTY");
	ttyFlag = ttyFlag.toLowerCase();
	if (ttyFlag != "no")	{
		// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
		if ((document.forms[0].ttyWindow == "undefined") || (document.forms[0].ttyWindow == "[object InputArray]"))
		{
			parent.controls.reloadDocument();
			return;
			}
		document.forms[0].ttyWindow.checked = parent.parent.globals.document.vars.ttyWindow.checked;
		}
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	// make sure all form element are valid objects, otherwise just skip & return!
	if (((document.forms[0].accountName == "undefined") || (document.forms[0].accountName == "[object InputArray]")) ||
		((document.forms[0].accountPassword == "undefined") || (document.forms[0].accountPassword == "[object InputArray]")) ||
		((document.forms[0].accountPasswordCheck == "undefined") || (document.forms[0].accountPasswordCheck == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	parent.parent.globals.document.vars.accountName.value = document.forms[0].accountName.value;
	parent.parent.globals.document.vars.accountPassword.value = document.forms[0].accountPassword.value;
	parent.parent.globals.document.vars.accountPasswordCheck.value = document.forms[0].accountPasswordCheck.value;

	var theFile = parent.parent.globals.getAcctSetupFilename(self);
	var ttyFlag = parent.parent.globals.GetNameValuePair(theFile,"Existing Acct Mode","AskTTY");
	ttyFlag = ttyFlag.toLowerCase();
	if (ttyFlag != "no")	{
		// make sure all form element are valid objects, otherwise just skip & return!
		if ((document.forms[0].ttyWindow == "undefined") || (document.forms[0].ttyWindow == "[object InputArray]")) {
			parent.controls.reloadDocument();
			return;
			}
		parent.parent.globals.document.vars.ttyWindow.checked = document.forms[0].ttyWindow.checked;
		}
	else	{
		parent.parent.globals.document.vars.ttyWindow.checked = 0;
		}
}



function generateTTYsupport()
{
	var theFile = parent.parent.globals.getAcctSetupFilename(self);
	ttyFlag = parent.parent.globals.GetNameValuePair(theFile,"Existing Acct Mode","AskTTY");
	ttyFlag = ttyFlag.toLowerCase();
	if (ttyFlag != "no")	{
		document.writeln("<INPUT NAME='ttyWindow' TYPE='checkbox'><P CLASS='tty'>I would like a terminal (TTY) window so that I can log in manually when I connect.</P>");
		}
}


// end hiding contents from old browsers  -->
