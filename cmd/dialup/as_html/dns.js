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
	if (document.forms[0].primaryDNS.value == "" && document.forms[0].secondaryDNS.value != "")	{
		document.forms[0].primaryDNS.value = document.forms[0].secondaryDNS.value;
		document.forms[0].secondaryDNS.value = "";
		}

	if (document.forms[0].primaryDNS.value != "")	{
		if (parent.parent.globals.verifyIPaddress(document.forms[0].primaryDNS.value)==false)	{
			alert("The address of the primary DNS server is not valid. It should consist of digits separated by periods.");
			document.forms[0].primaryDNS.focus();
			document.forms[0].primaryDNS.select();
			return(false);
			}
		}
	if (document.forms[0].secondaryDNS.value != "")	{
		if (parent.parent.globals.verifyIPaddress(document.forms[0].secondaryDNS.value)==false)	{
			alert("The address of the secondary DNS server is not valid. It should consist of digits separated by periods.");
			document.forms[0].secondaryDNS.focus();
			document.forms[0].secondaryDNS.select();
			return(false);
			}
		}
	return(true);
}



function loadData()
{
	// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
	if (((document.forms[0].domainName == "undefined") || (document.forms[0].domainName == "[object InputArray]")) ||
		((document.forms[0].primaryDNS == "undefined") || (document.forms[0].primaryDNS == "[object InputArray]")) ||
		((document.forms[0].secondaryDNS == "undefined") || (document.forms[0].secondaryDNS == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	document.forms[0].domainName.value = parent.parent.globals.document.vars.domainName.value;
	document.forms[0].primaryDNS.value = parent.parent.globals.document.vars.primaryDNS.value;
	document.forms[0].secondaryDNS.value = parent.parent.globals.document.vars.secondaryDNS.value;
	parent.parent.globals.setFocus(document.forms[0].domainName);
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	// make sure all form element are valid objects, otherwise just skip & return!
	if (((document.forms[0].domainName == "undefined") || (document.forms[0].domainName == "[object InputArray]")) ||
		((document.forms[0].primaryDNS == "undefined") || (document.forms[0].primaryDNS == "[object InputArray]")) ||
		((document.forms[0].secondaryDNS == "undefined") || (document.forms[0].secondaryDNS == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	parent.parent.globals.document.vars.domainName.value = document.forms[0].domainName.value;
	parent.parent.globals.document.vars.primaryDNS.value = document.forms[0].primaryDNS.value;
	parent.parent.globals.document.vars.secondaryDNS.value = document.forms[0].secondaryDNS.value;
}



// end hiding contents from old browsers  -->
