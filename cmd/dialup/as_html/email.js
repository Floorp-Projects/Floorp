/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
<!--  to hide script contents from old browsers



function go(msg)
{
	if (parent.parent.globals.document.vars.editMode.value == "yes")
		return true;
	else
		return(checkData());
}



function checkData()
{
	if (document.forms[0].emailPassword.value != document.forms[0].emailPasswordCheck.value)	{
		if (document.forms[0].emailPassword.value == "")	{
			parent.parent.globals.setFocus(document.forms[0].emailPassword);
			}
		else	{
			parent.parent.globals.setFocus(document.forms[0].emailPasswordCheck);
			}
		alert("The password you entered in 'Type Password Again' does not match the password you entered in 'Password'. Please re-enter your password.");
		return(false);
		}
	return(true);
}



function loadData()
{
	// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
	if (((document.forms[0].emailName == "undefined") || (document.forms[0].emailName == "[object InputArray]")) ||
		((document.forms[0].emailPassword == "undefined") || (document.forms[0].emailPassword == "[object InputArray]")) ||
		((document.forms[0].emailPasswordCheck == "undefined") || (document.forms[0].emailPasswordCheck == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	document.forms[0].emailName.value = parent.parent.globals.document.vars.emailName.value;
	document.forms[0].emailPassword.value = parent.parent.globals.document.vars.emailPassword.value;
	document.forms[0].emailPasswordCheck.value = parent.parent.globals.document.vars.emailPasswordCheck.value;
	if (document.forms[0].emailName.value == "" && document.forms[0].emailPassword.value == "" && document.forms[0].emailPasswordCheck.value == "")	{
		document.forms[0].emailName.value = parent.parent.globals.document.vars.accountName.value;
		document.forms[0].emailPassword.value = parent.parent.globals.document.vars.accountPassword.value;
		document.forms[0].emailPasswordCheck.value = parent.parent.globals.document.vars.accountPasswordCheck.value;
		}
	parent.parent.globals.setFocus(document.forms[0].emailName);
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	// make sure all form element are valid objects, otherwise just skip & return!
	if (((document.forms[0].emailName == "undefined") || (document.forms[0].emailName == "[object InputArray]")) ||
		((document.forms[0].emailPassword == "undefined") || (document.forms[0].emailPassword == "[object InputArray]")) ||
		((document.forms[0].emailPasswordCheck == "undefined") || (document.forms[0].emailPasswordCheck == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return(true);
	}

	parent.parent.globals.document.vars.emailName.value = document.forms[0].emailName.value;
	parent.parent.globals.document.vars.emailPassword.value = document.forms[0].emailPassword.value;
	parent.parent.globals.document.vars.emailPasswordCheck.value = document.forms[0].emailPasswordCheck.value;
	return(true);
}



// end hiding contents from old browsers  -->
