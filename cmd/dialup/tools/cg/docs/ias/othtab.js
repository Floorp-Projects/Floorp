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
var loaded = false;
//the file that includes this must also include settings.js and nciglob.js

function loadData()
{
	if (parent.parent.iasglobals && parent.parent.iasglobals.getText)
	{
		var FormName = "IASOTHER";
	
		parent.parent.iasglobals.setCheckBox(FormName, "SECURITYDEVICE", parent.parent.iasglobals.getGlobal("SecurityDevice"));	
		//parent.parent.iasglobals.setCheckBox(FormName, "REGSCRIPTING", parent.parent.iasglobals.getGlobal("REG_SCRIPTING"));		
		this.focus();
		document.forms[0]["SECURITYDEVICE"].focus();
		loaded = true;
	}
	else
		setTimeout("loadData()",500);

}//function loadData()


function checkData()
{
	return true;
}


function saveData()
{
	if (loaded && loaded == true)
	{
		var FormName = "IASOTHER";
	
		parent.parent.iasglobals.setGlobal("SecurityDevice", parent.parent.iasglobals.getCheckBox(FormName, "SECURITYDEVICE"));
		//parent.parent.iasglobals.setGlobal("REG_SCRIPTING", parent.parent.iasglobals.getCheckBox(FormName, "REGSCRIPTING"));
	}
}

// end hiding contents from old browsers  -->
