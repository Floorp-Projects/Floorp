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

//the file that includes this must also include gettings.js and nciglob.js
var loaded = false;

function loadData()
{
	if (parent.parent.iasglobals && parent.parent.iasglobals.getGlobal)
	{
		var FormName = "IASCONFIGNAME";
	
		parent.parent.iasglobals.setText(FormName, "configName", parent.parent.iasglobals.getGlobal("SiteName"));			

		this.focus();
		document.forms[0]["configName"].focus();
		loaded = true;
	}
	else
		setTimeout("loadData()",500);
}//function loadData()


function checkData()
{
	var valid = true;
	
	if (loaded && loaded == true)
	{
		if (parent.parent.iasglobals.iasDirty(null))
		{
			var FormName = "IASCONFIGNAME";
		
			var theName = parent.parent.iasglobals.getText(FormName, "configName");
			if (theName == null || theName == "" || theName == "null")
			{
				var override = !(confirm("Please Enter a Configuration Name before Continuing, or choose cancel to ignore changes."));
				document.forms[FormName]["configName"].focus();
				document.forms[FormName]["configName"].select();
				valid = override;
			}
		
		}
	
		if (override)
			parent.parent.iasglobals.iasDirty(false);

	}
	return valid;
}


function saveData()
{
	if (loaded && loaded == true)
	{
		var FormName = "IASCONFIGNAME";
	
		parent.parent.iasglobals.setGlobal("SiteName", parent.parent.iasglobals.getText(FormName, "configName"));
		parent.parent.iasglobals.setGlobal("Description", parent.parent.iasglobals.getText(FormName, "configName"));
	}
}

// end hiding contents from old browsers  -->
