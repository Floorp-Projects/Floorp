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
	if (document.forms[0].first.value == "")	{
		alert("You must enter a first name.");
		document.forms[0].first.focus();
		document.forms[0].first.select();
		return false;
		}
	if (document.forms[0].last.value == "")	{
		alert("You must enter a last name.");
		document.forms[0].last.focus();
		document.forms[0].last.select();
		return false;
		}
/*
	if (document.forms[0].areaCode.value == "")	{
		alert("You must enter an area code.");
		document.forms[0].areaCode.focus();
		document.forms[0].areaCode.select();
		return false;
		}
	if (document.forms[0].phoneNumber.value == "")	{
		alert("You must enter a telephone number.");
		document.forms[0].phoneNumber.focus();
		document.forms[0].phoneNumber.select();
		return false;
		}
*/
	return true;
}



function loadData()
{
	// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
	if (((document.forms[0].first == "undefined") || (document.forms[0].first == "[object InputArray]")) ||
		((document.forms[0].last == "undefined") || (document.forms[0].last == "[object InputArray]")) ||
		((document.forms[0].company == "undefined") || (document.forms[0].company == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	document.forms[0].first.value = parent.parent.globals.document.vars.first.value;
	document.forms[0].last.value = parent.parent.globals.document.vars.last.value;
	document.forms[0].company.value = parent.parent.globals.document.vars.company.value;
//	document.forms[0].areaCode.value = parent.parent.globals.document.vars.areaCode.value;
//	document.forms[0].phoneNumber.value = parent.parent.globals.document.vars.phoneNumber.value;
	parent.parent.globals.setFocus(document.forms[0].first);
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	// make sure all form element are valid objects, otherwise just skip & return!
	if (((document.forms[0].first == "undefined") || (document.forms[0].first == "[object InputArray]")) ||
		((document.forms[0].last == "undefined") || (document.forms[0].last == "[object InputArray]")) ||
		((document.forms[0].company == "undefined") || (document.forms[0].company == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	parent.parent.globals.document.vars.first.value = document.forms[0].first.value;
	parent.parent.globals.document.vars.last.value = document.forms[0].last.value;
	parent.parent.globals.document.vars.company.value = document.forms[0].company.value;
//	parent.parent.globals.document.vars.areaCode.value = document.forms[0].areaCode.value;
//	parent.parent.globals.document.vars.phoneNumber.value = document.forms[0].phoneNumber.value;
}



// end hiding contents from old browsers  -->
