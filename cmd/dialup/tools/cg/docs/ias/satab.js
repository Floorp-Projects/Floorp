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

//the file that includes this must also include settings.js and iasglob.js
var loaded = false;
function loadData()
{
	if (parent.parent.iasglobals && parent.parent.iasglobals.getText)
	{
		var FormName = "IASACCOUNTACCESS";
		
		
		parent.parent.iasglobals.setText(FormName, "IASPHONE", parent.parent.iasglobals.getGlobal("Phone"));			
		
		parent.parent.iasglobals.setText(FormName, "IASLOGIN", parent.parent.iasglobals.getGlobal("Name"));			
		parent.parent.iasglobals.setText(FormName, "IASPASSWORD", parent.parent.iasglobals.getGlobal("Password"));
		parent.parent.iasglobals.setText(FormName, "REGCGI", parent.parent.iasglobals.getGlobal("RegCGI"));
		this.focus();
		document.forms[0]["IASPHONE"].focus();
		loaded = true;	
	}
	else
		setTimeout("loadData()",500);
	
}//function loadData()


function checkData()
{
	if (loaded && loaded == true)
	{

		if (parent.parent.iasglobals.iasDirty(null))
		{
			var FormName	= "IASACCOUNTACCESS";
			var valid 		= true;
			var tempData 	= "";
			var override	= false;
		
			tempData = parent.parent.iasglobals.getText(FormName, "IASPHONE");
			if (tempData == null || tempData == "" || tempData == "null")
			{
				override = !confirm("Please enter the phone number for the Internet Account Server, or choose cancel to ignore changes.");
				document.forms[FormName]["IASPHONE"].focus();
				document.forms[FormName]["IASPHONE"].select();
				valid = override;
			}
			
			if (valid == true && override == false)
			{
				tempData = parent.parent.iasglobals.getText(FormName, "REGCGI");
				if (tempData == null || tempData == "" || tempData == "null")
				{
					override = !confirm("Please enter the URL for the Internet Account Server's CGI, or choose cancel to ignore changes.");
					document.forms[FormName]["REGCGI"].focus();
					document.forms[FormName]["REGCGI"].select();
					valid = override;
				}
			}
		
			if (valid == true && override == false)
			{
				tempData = parent.parent.iasglobals.getText(FormName, "IASLOGIN");
				if (tempData == null || tempData == "" || tempData == "null")
				{
					//valid = confirm("You have not entered a login name for dialing into the Internet Account Server.  Are you sure you don't need one? (If no login name is necessary, choose \"OK\")");
					//if (valid == false)
	
					override = !confirm("Please enter the login name required to dial into the Internet Account Server, or choose cancel to ignore changes.");
					valid = override;
					{
						document.forms[FormName]["IASLOGIN"].focus();
						document.forms[FormName]["IASLOGIN"].select();
					}
				}
			}
			
			
			if (valid == true && override == false)
			{
				tempData = parent.parent.iasglobals.getText(FormName, "IASPASSWORD");
				if (tempData == null || tempData == "" || tempData == "null")
				{
					//valid = confirm("You have not entered a password for dialing into the Internet Account Server.  Are you sure you don't need one? (If no password is necessary, choose \"OK\")");
		
					//if (valid == false)
					override = !confirm("Please enter the password required to dial into the Internet Account Server, or choose cancel to ignore changes.");
					valid = false;
					{
						document.forms[FormName]["IASPASSWORD"].focus();
						document.forms[FormName]["IASPASSWORD"].select();
					}
				}
			}
			
			if (override == true)
				parent.parent.iasglobals.iasDirty(false);
		
			return valid;
		}
		else
			return true;
	}
	else
		return true;
}


function saveData()
{
	if (loaded && loaded == true)
	{
		var FormName = "IASACCOUNTACCESS";
		
		parent.parent.iasglobals.setGlobal("Phone", parent.parent.iasglobals.getText(FormName, "IASPHONE"));
		parent.parent.iasglobals.setGlobal("Name", parent.parent.iasglobals.getText(FormName, "IASLOGIN"));
		parent.parent.iasglobals.setGlobal("Password", parent.parent.iasglobals.getText(FormName, "IASPASSWORD"));
		parent.parent.iasglobals.setGlobal("RegCGI", parent.parent.iasglobals.getText(FormName, "REGCGI"));
	
	}
}

function encryptPassword()
{


	var FormName = "IASACCOUNTACCESS";
	var passwd = parent.parent.iasglobals.getText(FormName, "IASPASSWORD");

	if (passwd && passwd != null && passwd.length > 0)
	{
		if (passwd.length < 20)
		{
			netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");
			var encPass = top.globals.document.setupPlugin.EncryptPassword(passwd);
			parent.parent.iasglobals.setText(FormName, "IASPASSWORD", encPass);
		}		
	} 
}

// end hiding contents from old browsers  -->
