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
		var FormName = "NCISERVERS";
		
		
		parent.parent.nciglobals.setText(FormName, "DOMAIN", parent.parent.nciglobals.getGlobal("DomainName"));			
		
		parent.parent.nciglobals.setText(FormName, "DNS1", parent.parent.nciglobals.getGlobal("DNSAddress"));			
		parent.parent.nciglobals.setText(FormName, "DNS2", parent.parent.nciglobals.getGlobal("DNSAddress2"));
		
		var ip = parent.parent.nciglobals.getGlobal("IPAddress");
		if ((ip != null) && (ip != "") && (ip != "0.0.0.0"))
		{
			parent.parent.nciglobals.setCheckBox(FormName, "DynamicIP", "no");
			parent.parent.nciglobals.setText(FormName, "IPADDRESS", ip);
		}
		else
		{
			parent.parent.nciglobals.setCheckBox(FormName, "DynamicIP", "yes");
			parent.parent.nciglobals.setText(FormName, "IPADDRESS", "0.0.0.0");
		}
		
		parent.parent.nciglobals.setText(FormName, "SMTP", parent.parent.nciglobals.getGlobal("SMTP_Server"));			
		parent.parent.nciglobals.setText(FormName, "NNTP", parent.parent.nciglobals.getGlobal("NNTP_Server"));
		parent.parent.nciglobals.setText(FormName, "POP", parent.parent.nciglobals.getGlobal("POP_Server"));
		parent.parent.nciglobals.setText(FormName, "IMAP", parent.parent.nciglobals.getGlobal("IMAP_Server"));
		
		var popOrImap = parent.parent.nciglobals.getGlobal("Default_Mail_Protocol");
		if (popOrImap == "IMAP")
			parent.parent.nciglobals.setRadio(FormName, "POPorIMAP", 1);
		else
			parent.parent.nciglobals.setRadio(FormName, "POPorIMAP", 0);
		
		
		//parent.parent.nciglobals.setText(FormName, "LDAP", parent.parent.nciglobals.getGlobal("LDAP_Servers"));			
		this.focus();
		document.forms[0]["DOMAIN"].focus();
		loaded = true;
	}
	else
		setTimeout("loadData()",500);

}//function loadData()


function checkData()
{

	var FormName	= "NCISERVERS";
	var valid 		= true;
	var tempData 	= "";
	var override 	= false;
	
	if (loaded && loaded == true)
	{
		{	
			tempData = parent.parent.nciglobals.getText(FormName, "DNS1");
			if ((tempData != null && tempData != "" && tempData != "null") && (top.globals.verifyIPaddress && top.globals.verifyIPaddress(tempData) == false))
			{
				override = !confirm("Please enter a valid IP address, or choose cancel to ignore changes.");
				document.forms[FormName]["DNS1"].focus();
				document.forms[FormName]["DNS1"].select();
				valid = override;
			}	
		}
	
		if (valid == true && override == false)
		{	
			tempData = parent.parent.nciglobals.getText(FormName, "DNS2");
			if ((tempData != null && tempData != "" && tempData != "null") && (top.globals.verifyIPaddress && top.globals.verifyIPaddress(tempData) == false))
			{
				overrride = !confirm("Please enter a valid IP address, or choose cancel to ignore changes");
				document.forms[FormName]["DNS2"].focus();
				document.forms[FormName]["DNS2"].select();
				valid = override;
			}	
		}
	
		if (valid == true && override == false && (parent.parent.nciglobals.getCheckBox(FormName,"DynamicIP")=="no"))
		{	
			tempData = parent.parent.nciglobals.getText(FormName, "IPADDRESS");
			if ((tempData != null && tempData != "" && tempData != "null") && (top.globals.verifyIPaddress && top.globals.verifyIPaddress(tempData) == false))
			{
				overrride = !confirm("Please enter a valid IP address, or choose cancel to ignore changes");
				document.forms[FormName]["IPADDRESS"].focus();
				document.forms[FormName]["IPADDRESS"].select();
				valid = override;
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
		var FormName = "NCISERVERS";
		
		parent.parent.nciglobals.setGlobal("DomainName", parent.parent.nciglobals.getText(FormName, "DOMAIN"));
		parent.parent.nciglobals.setGlobal("DNSAddress", parent.parent.nciglobals.getText(FormName, "DNS1"));
		parent.parent.nciglobals.setGlobal("DNSAddress2", parent.parent.nciglobals.getText(FormName, "DNS2"));
		
		if (parent.parent.nciglobals.getCheckBox(FormName, "DynamicIP") == "no")
			var ip = parent.parent.nciglobals.getText(FormName, "IPADDRESS");
			if ((ip != null) && (ip != "") && (true))	//replace this true with an ip checker fcn
				parent.parent.nciglobals.setGlobal("IPAddress", ip);
		else
		{
			parent.parent.nciglobals.setGlobal("IPAddress", "0.0.0.0");
		}
		parent.parent.nciglobals.setGlobal("SMTP_Server", parent.parent.nciglobals.getText(FormName, "SMTP"));
		parent.parent.nciglobals.setGlobal("NNTP_Server", parent.parent.nciglobals.getText(FormName, "NNTP"));
		parent.parent.nciglobals.setGlobal("POP_Server", parent.parent.nciglobals.getText(FormName, "POP"));
		parent.parent.nciglobals.setGlobal("IMAP_Server", parent.parent.nciglobals.getText(FormName, "IMAP"));
	
		var popOrImap = parent.parent.nciglobals.getRadio(FormName, "POPorIMAP");
		
		if (popOrImap == "no") // 2nd option = IMAP
		{
			parent.parent.nciglobals.setGlobal("Default_Mail_Protocol", "IMAP");
		}
		else
		{
			parent.parent.nciglobals.setGlobal("Default_Mail_Protocol", "POP");
		}	
	
	//	parent.parent.nciglobals.setGlobal("LDAP_Servers", getText(FormName, "LDAP"));
	}
}


//handles clicking of the checkbox
function handleCheck()
{
	var FormName = "NCISERVERS";
	var checked = parent.parent.nciglobals.getCheckBox(FormName, "DynamicIP");
	
	if (checked == "yes")
	{
		parent.parent.nciglobals.setText(FormName, "IPADDRESS", "0.0.0.0");
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
	var FormName = "NCISERVERS";
	var ip = parent.parent.nciglobals.getText(FormName, "IPADDRESS");
	
	var ip = parent.parent.nciglobals.getText(FormName, "IPADDRESS");
	if ((ip != null) && (ip != "") && (ip != "0.0.0.0"))
	{
		parent.parent.nciglobals.setGlobal("IPAddress", ip);
		parent.parent.nciglobals.setCheckBox(FormName, "DynamicIP", "no");
	}
	else
	{
		parent.parent.nciglobals.setCheckBox(FormName, "DynamicIP", "yes");
		parent.parent.nciglobals.setText(FormName, "IPADDRESS", "0.0.0.0");
		parent.parent.nciglobals.setGlobal("IPAddress", "0.0.0.0");
	}

}

//if either pop or imap is empty, this sets the default radio button to the other one
function handleMailChange()
{
	var FormName = "NCISERVERS";
	var pop = parent.parent.nciglobals.getText(FormName, "POP");
	var imap = parent.parent.nciglobals.getText(FormName, "IMAP");
	
	if ((imap == "") && (pop != ""))
	{
		parent.parent.nciglobals.setRadio(FormName, "POPorIMAP", 0);	//set it to the first option, POP
	}
	else if ((imap != "") && (pop == ""))
	{
		parent.parent.nciglobals.setRadio(FormName, "POPorIMAP", 1);
	
	}
}

// end hiding contents from old browsers  -->
