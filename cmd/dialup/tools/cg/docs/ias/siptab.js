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
		var FormName = "IASSERVERS";
		
		
		parent.parent.iasglobals.setText(FormName, "DOMAIN", parent.parent.iasglobals.getGlobal("DomainName"));			
		
		parent.parent.iasglobals.setText(FormName, "DNS1", parent.parent.iasglobals.getGlobal("DNSAddress"));			
		parent.parent.iasglobals.setText(FormName, "DNS2", parent.parent.iasglobals.getGlobal("DNSAddress2"));
		
		var ip = parent.parent.iasglobals.getGlobal("IPAddress");
		if ((ip != null) && (ip != "") && (ip != "0.0.0.0"))
		{
			parent.parent.iasglobals.setCheckBox(FormName, "DynamicIP", "no");
			parent.parent.iasglobals.setText(FormName, "IPADDRESS", ip);
		}
		else
		{
			parent.parent.iasglobals.setCheckBox(FormName, "DynamicIP", "yes");
			parent.parent.iasglobals.setText(FormName, "IPADDRESS", "0.0.0.0");
		}
	
		this.focus();
		document.forms[0]["DOMAIN"].focus();
		loaded = true;
	}
	else
		setTimeout("loadData()",500);
}//function loadData()


function checkData()
{

	
	if (loaded && loaded == true && parent.parent.iasglobals.iasDirty(null))
	{
		var FormName	= "IASSERVERS";
		var valid 		= true;
		var tempData 	= "";
		var override 	= false;
	
		tempData = parent.parent.iasglobals.getText(FormName, "DOMAIN");
		if (tempData == null || tempData == "" || tempData == "null")
		{
			override = !confirm("Please enter a domain name, or choose cancel to ignore changes.");
			document.forms[FormName]["DOMAIN"].focus();
			document.forms[FormName]["DOMAIN"].select();
			valid = override;
		}
	
	
		if (valid == true && override == false)
		{	
			tempData = parent.parent.iasglobals.getText(FormName, "DNS1");
			if ((tempData == null || tempData == "" || tempData == "null") || (top.globals.verifyIPaddress && top.globals.verifyIPaddress(tempData) == false))
			{
				override = !confirm("Please enter a valid IP address, or choose cancel to ignore changes.");
				document.forms[FormName]["DNS1"].focus();
				document.forms[FormName]["DNS1"].select();
				valid = override;
			}	
		}
	
		if (valid == true && override == false)
		{	
			tempData = parent.parent.iasglobals.getText(FormName, "DNS2");
			if ((tempData != null && tempData != "" && tempData != "null") && (top.globals.verifyIPaddress && top.globals.verifyIPaddress(tempData) == false))
			{
				override = !confirm("Please enter a valid IP address, or choose cancel to ignore changes.");
				document.forms[FormName]["DNS2"].focus();
				document.forms[FormName]["DNS2"].select();
				valid = override;
			}	
		}
		
		if ((valid == true)  && override == false && (parent.parent.iasglobals.getCheckBox(FormName,"DynamicIP")=="no"))
		{	
			tempData = parent.parent.iasglobals.getText(FormName, "IPADDRESS");
			if ((tempData == null || tempData == "" || tempData == "null") || (top.globals.verifyIPaddress && top.globals.verifyIPaddress(tempData) == false))
			{
				override = !confirm("Please enter a valid IP address, or choose cancel to ignore changes.");
				document.forms[FormName]["IPADDRESS"].focus();
				document.forms[FormName]["IPADDRESS"].select();
				valid = override;
			}	
			
		}
		
		if (override == true)
			parent.parent.iasglobals.iasDirty(false);
			
		return valid;
	}
	else
		return true;
}


function saveData()
{
	if (loaded && loaded == true)
	{
		var FormName = "IASSERVERS";
		
		parent.parent.iasglobals.setGlobal("DomainName", parent.parent.iasglobals.getText(FormName, "DOMAIN"));
		parent.parent.iasglobals.setGlobal("DNSAddress", parent.parent.iasglobals.getText(FormName, "DNS1"));
		parent.parent.iasglobals.setGlobal("DNSAddress2", parent.parent.iasglobals.getText(FormName, "DNS2"));
		
		if (parent.parent.iasglobals.getCheckBox(FormName, "DynamicIP") == "no")
			var ip = parent.parent.iasglobals.getText(FormName, "IPADDRESS");
			if ((ip != null) && (ip != "") && (true))	//replace this true with an ip checker fcn
				parent.parent.iasglobals.setGlobal("IPAddress", ip);
		else
		{
			parent.parent.iasglobals.setGlobal("IPAddress", "0.0.0.0");
		}
	}
}


//handles clicking of the checkbox
function handleCheck()
{
	
	var FormName = "IASSERVERS";
	var checked = parent.parent.iasglobals.getCheckBox(FormName, "DynamicIP");
	
	if (checked == "yes")
	{
		parent.parent.iasglobals.setText(FormName, "IPADDRESS", "0.0.0.0");
	}
	else
	{
		document[FormName].IPADDRESS.focus();
		document[FormName].IPADDRESS.select();
	}
}

//handle writing in the ip field
function handleIPChange()
{
	var FormName = "IASSERVERS";
	var ip = parent.parent.iasglobals.getText(FormName, "IPADDRESS");
	
	var ip = parent.parent.iasglobals.getText(FormName, "IPADDRESS");
	if ((ip != null) && (ip != "") && (ip != "0.0.0.0"))
	{
		parent.parent.iasglobals.setGlobal("IPAddress", ip);
		parent.parent.iasglobals.setCheckBox(FormName, "DynamicIP", "no");
	}
	else
	{
		parent.parent.iasglobals.setCheckBox(FormName, "DynamicIP", "yes");
		parent.parent.iasglobals.setText(FormName, "IPADDRESS", "0.0.0.0");
		parent.parent.iasglobals.setGlobal("IPAddress", "0.0.0.0");
	}

}



// end hiding contents from old browsers  -->
