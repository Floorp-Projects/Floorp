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
	if (parent.parent.nciglobals && parent.parent.nciglobals.getText)
	{
	
		var FormName = "NCIACCOUNTACCESS";
		
		
		parent.parent.nciglobals.setText(FormName, "ACCESSPHONE", parent.parent.nciglobals.getGlobal("Phone"));			
		
		parent.parent.nciglobals.setText(FormName, "ACCOUNTLOGIN", parent.parent.nciglobals.getGlobal("Name"));			
		parent.parent.nciglobals.setText(FormName, "ACCOUNTPASSWORD", parent.parent.nciglobals.getGlobal("Password"));			
		
		//parent.parent.nciglobals.setText(FormName, "EMAILLOGIN", parent.parent.nciglobals.getGlobal("Email_Name"));			
		//parent.parent.nciglobals.setText(FormName, "EMAILPASSWORD", parent.parent.nciglobals.getGlobal("Email_Password"));			
	
		parent.parent.nciglobals.setText(FormName, "REGPODURL", parent.parent.nciglobals.getGlobal("RegPodURL"));			

		var tempFlag = parent.parent.nciglobals.getGlobal("EnableVJCompression");
		if (!tempFlag || tempFlag == null || tempFlag == "" || tempFlag == "null" || tempFlag.toLowerCase() != "no")
			{
				tempFlag = "yes";
			}
		parent.parent.nciglobals.setCheckBox(FormName, "EnableVJCompression", tempFlag);			
		
		this.focus();
		document.forms[0]["ACCESSPHONE"].focus();
		loaded = true;

	}
	else
		setTimeout("loadData()",500);
}//function loadData()


function checkData()
{
	var FormName = "NCIACCOUNTACCESS";
	var override 	= false;
	var valid = true;
	var phoneNum = parent.parent.nciglobals.getText(FormName,"ACCESSPHONE");

	if (loaded && loaded == true)
	{
		if(phoneNum.length > 0)
		{
			for (var x=0; x<phoneNum.length; x++)	
			{
				if ("0123456789().-+ ".indexOf(phoneNum.charAt(x)) <0)	{
					override = !confirm("The phone number is not valid.  Please enter a vaild phone number, or choose cancel to ignore changes.");
					document.forms[FormName]["ACCESSPHONE"].focus();
					document.forms[FormName]["ACCESSPHONE"].select();
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
		var FormName = "NCIACCOUNTACCESS";
		
		parent.parent.nciglobals.setGlobal("Phone", parent.parent.nciglobals.getText(FormName, "ACCESSPHONE"));
		parent.parent.nciglobals.setGlobal("Name", parent.parent.nciglobals.getText(FormName, "ACCOUNTLOGIN"));
		parent.parent.nciglobals.setGlobal("Password", parent.parent.nciglobals.getText(FormName, "ACCOUNTPASSWORD"));
		//parent.parent.nciglobals.setGlobal("Email_Name", parent.parent.nciglobals.getText(FormName, "EMAILLOGIN"));
		//parent.parent.nciglobals.setGlobal("Email_Password", parent.parent.nciglobals.getText(FormName, "EMAILPASSWORD"));
		parent.parent.nciglobals.setGlobal("RegPodURL", parent.parent.nciglobals.getText(FormName, "REGPODURL"));
		parent.parent.nciglobals.setGlobal("EnableVJCompression", parent.parent.nciglobals.getCheckBox(FormName, "EnableVJCompression"));
	}
}

// end hiding contents from old browsers  -->
