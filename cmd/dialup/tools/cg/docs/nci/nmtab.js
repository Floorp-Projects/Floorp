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

//the file that includes this must also include settings.js and nciglob.js
var loaded = false;

function loadData()
{
	if (parent.parent.nciglobals && parent.parent.nciglobals.getGlobal)
	{
		var FormName = "NCICONFIGNAME";
	
		parent.parent.nciglobals.setText(FormName, "configName", parent.parent.nciglobals.getGlobal("SiteName"));			
		parent.parent.nciglobals.setText(FormName, "supportNumber", parent.parent.nciglobals.getGlobal("SupportPhone"));			
		this.focus();
		document.forms[0]["configName"].focus();
		loaded = true;
	}
	else
		setTimeout("loadData()",500);
}//function loadData()


function checkData()
{
	var FormName = "NCICONFIGNAME";
	var override 	= false;
	var valid = true;
	var phoneNum = parent.parent.nciglobals.getText(FormName,"supportNumber");

	if (loaded && loaded == true)
	{
		if(phoneNum.length > 0)
		{
			for (var x=0; x<phoneNum.length; x++)	
			{
				if ("0123456789().-+ ".indexOf(phoneNum.charAt(x)) <0)	{
					override = !confirm("The Support Phone Number is not valid.  Please enter a vaild phone number, or choose cancel to ignore changes.");
					document.forms[FormName]["supportNumber"].focus();
					document.forms[FormName]["supportNumber"].select();
					valid = override;
					break;
					}
			}
		}
	
		if (override == true)
			parent.parent.nciglobals.nciDirty(false);
	}
	
	return valid;
}


function saveData()
{
	if (loaded && loaded == true)
	{
		var FormName = "NCICONFIGNAME";
	
		parent.parent.nciglobals.setGlobal("SiteName", parent.parent.nciglobals.getText(FormName, "configName"));
		parent.parent.nciglobals.setGlobal("Description", parent.parent.nciglobals.getText(FormName, "configName"));
		parent.parent.nciglobals.setGlobal("SupportPhone", parent.parent.nciglobals.getText(FormName,"supportNumber"));
	}
}

// end hiding contents from old browsers  -->
