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



function go(msg)
{
	if (parent.parent.globals.document.vars.editMode.value == "yes")	{
		return(true);
		}
	else	{
		return(checkData());
		}
}



function checkData()
{
	var theIndex = document.forms[0].providerlist.selectedIndex;
	if (theIndex < 0)	{
		alert("Please select from the list before continuing.");
		parent.parent.globals.setFocus(document.forms[0].providerlist);
		return(false);
		}
	var theProviderName = document.forms[0].providerlist.options[theIndex].text;
	var theProviderFilename = document.forms[0].providerlist.options[theIndex].value;
	if (theProviderFilename == "")	{
		theProviderName = "";
		parent.parent.globals.debug("User chose none of the above.");
		}
	else	{
		parent.parent.globals.debug("ISP Name: " + theProviderName);
		parent.parent.globals.debug("ISP Filename: " + theProviderFilename);
		}
	return(true);
}



function loadData()
{
	// make sure all data objects/element exists and valid; otherwise, reload.

	if ((document.forms[0].providerlist == "undefined") || (document.forms[0].providerlist == "[object InputArray]"))	{
		parent.controls.reloadDocument();
		return;
		}

	parent.parent.globals.setFocus(document.forms[0].providerlist);
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	// make sure all data objects/element exists and valid; otherwise, reload.

	if ((document.forms[0].providerlist == "undefined") || (document.forms[0].providerlist == "[object InputArray]"))	{
		parent.controls.reloadDocument();
		return;
		}

	var theFile = parent.parent.globals.getAcctSetupFilename(self);
	var intlFlag = parent.parent.globals.GetNameValuePair(theFile,"Mode Selection","IntlMode");
	if (intlFlag != null && intlFlag != "")	{
		intlFlag = intlFlag.toLowerCase();
		}

	var theProviderName = "";
	var theProviderFilename = "";
	if (document.forms[0].providerlist.selectedIndex >=0)	{
		theProviderName = document.forms[0].providerlist.options[document.forms[0].providerlist.selectedIndex].text;
		theProviderFilename = document.forms[0].providerlist.options[document.forms[0].providerlist.selectedIndex].value;
		if (theProviderFilename == "")	{
			theProviderName = "";
			}
		}
	parent.parent.globals.document.vars.providername.value = theProviderName;
	parent.parent.globals.document.vars.providerFilename.value = theProviderFilename;

	// clear fields before reading in data from .NCI file
	
	parent.parent.globals.document.vars.accountAreaCode.value = "";
	parent.parent.globals.document.vars.accountPhoneNumber.value = "";
	parent.parent.globals.document.vars.domainName.value = "";
	parent.parent.globals.document.vars.primaryDNS.value = "";
	parent.parent.globals.document.vars.secondaryDNS.value = "";
	parent.parent.globals.document.vars.ipAddress.value = "";
	parent.parent.globals.document.vars.SMTP.value = "";
	parent.parent.globals.document.vars.mailServer.value = "";
	parent.parent.globals.document.vars.mailProtocol.value = "";
	parent.parent.globals.document.vars.NNTP.value = "";
	parent.parent.globals.document.vars.publishURL.value = "";
	parent.parent.globals.document.vars.publishPassword.value = "";
	parent.parent.globals.document.vars.viewURL.value = "";
	parent.parent.globals.document.vars.scriptEnabled.value = "";
	parent.parent.globals.document.vars.scriptFile.value = "";
	parent.parent.globals.document.vars.lckFilename.value = "";

	var data="";
	if (theProviderFilename != "")	{
		
		// read default values from selected .NCI file

		data = "" + parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Dial-In Configuration","Phone");
		if (data != null && data != "")	{
			parent.parent.globals.document.vars.accountAreaCode.value = "";
			parent.parent.globals.document.vars.accountPhoneNumber.value = data;
			if (intlFlag != "yes")	{
				var x = data.indexOf("(");
				if (x>=0)	{
					var y = data.indexOf(")");
					if (y>x)	{
						var areaCode = data.substring(x+1,y);
						data = data.substring(y+1,data.length);
						if (data.charAt(0) == ' ')	{
							data = data.substring(1,data.length);
							}
						parent.parent.globals.document.vars.accountAreaCode.value = areaCode;
						parent.parent.globals.document.vars.accountPhoneNumber.value = data;
						}
					}
				}
			}

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"IP","DomainName");
		parent.parent.globals.debug("ISP DomainName: " + data);
		parent.parent.globals.document.vars.domainName.value = data;

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"IP","DNSAddress");
		parent.parent.globals.debug("ISP DNSAddress: " + data);
		parent.parent.globals.document.vars.primaryDNS.value = data;

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"IP","DNSAddress2");
		parent.parent.globals.debug("ISP DNSAddress2: " + data);
		parent.parent.globals.document.vars.secondaryDNS.value = data;

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"IP","IPAddress");
		parent.parent.globals.debug("ISP IPAddress: " + data);
		parent.parent.globals.document.vars.ipAddress.value = data;

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Services","SMTP_Server");
		parent.parent.globals.debug("ISP smtpHost: " + data);
		parent.parent.globals.document.vars.SMTP.value = data;

		parent.parent.globals.document.vars.mailServer.value = "";
/*
		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Services","POP_Server");
		parent.parent.globals.debug("ISP popHost: " + data);
		parent.parent.globals.document.vars.POP.value = data;

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Services","IMAP_Server");
		parent.parent.globals.debug("ISP imapHost: " + data);
		parent.parent.globals.document.vars.IMAP.value = data;
*/

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Services","Default_Mail_Protocol");
		parent.parent.globals.debug("ISP Default_Mail_Protocol: " + data);
		parent.parent.globals.document.vars.mailProtocol.value = data;

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Services","NNTP_Server");
		parent.parent.globals.debug("ISP nntpHost: " + data);
		parent.parent.globals.document.vars.NNTP.value = data;

/*
		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Services","NUM_LDAP_Servers");
		if (data != null && data != "")	{
			var numLDAPHosts = parseInt(data);
			if (numLDAPHosts>0)	{
				var LDAPdata = "";
				for (var x=1; x<=numLDAPHosts; x++)	{
					var theLDAPstring = "LDAP_Server_" + x;
					data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Services",theLDAPstring);
					if (data != null && data != "")	{
						parent.parent.globals.debug("ISP LDAP_Server_" + x + ": " + data);
						LDAPdata += data + "\r";
						}
					}
				parent.parent.globals.document.vars.LDAP.value = LDAPdata;
				}
			}
*/
		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Publishing","Publish_URL");
		parent.parent.globals.debug("ISP Publish_URL: " + data);
		parent.parent.globals.document.vars.publishURL.value = data;

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Publishing","Publish_Password");
		parent.parent.globals.debug("ISP Publish_Password: " + data);
		parent.parent.globals.document.vars.publishPassword.value = data;

		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Publishing","View_URL");
		parent.parent.globals.debug("ISP View_URL: " + data);
		parent.parent.globals.document.vars.viewURL.value = data;
	
		// scripting support
	
		var theScriptFile = "";
		var theScriptEnabledFlag = "FALSE";
		data = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Script","ScriptEnabled");
		if (data != null && data != "")	{
			data = data.toLowerCase();
			}
		if (data == "yes")	{
			theScriptFile = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Script","ScriptFileName");
			if (theScriptFile != null && theScriptFile != "")	{
				theScriptEnabledFlag = "TRUE";
				theScriptFile = parent.parent.globals.getConfigFolder(self) + theScriptFile;
				parent.parent.globals.debug("ISP ScriptFileName: " + theScriptFile);
				}
			}
		parent.parent.globals.document.vars.scriptEnabled.value = theScriptEnabledFlag;
		parent.parent.globals.document.vars.scriptFile.value = theScriptFile;
	
		// profile lockfile support
		
		parent.parent.globals.document.vars.lckFilename.value = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Configuration","ConfigurationFileName");
		}
}



function ISP(theProviderFilename,name)
{
	this.theProviderFilename=theProviderFilename;
	this.name=name;
}



function ISPcompare(a,b)
{
	if (a.name < b.name)	return(-1);
	else if (a.name == b.name)	return(0);
	return(1);
}



function generateISPList()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	for (x=document.forms[0].providerlist.length; x>=0; x--)	{
		document.forms[0].providerlist[x]=null;
		}

	var theAcctSetupFile = parent.parent.globals.getAcctSetupFilename(self);
	var showPhonesFlag = parent.parent.globals.GetNameValuePair(theAcctSetupFile,"Existing Acct Mode","ShowPhones");
	if (showPhonesFlag != null && showPhonesFlag != "")	{
		showPhonesFlag = showPhonesFlag.toLowerCase();
		}

	var pathName = parent.parent.globals.getConfigFolder(self);
	var theList = parent.parent.globals.document.setupPlugin.GetFolderContents(pathName,".NCI");

	if (theList != null)	{
		parent.parent.globals.debug("GetFolderContents returned " + theList.length + " items");

		var ISParray = new Array();
		for (var i=0,j=0; i<theList.length; i++)	{
			var theProviderFilename = pathName + theList[i];
//			parent.parent.globals.debug("theProviderFilename " + i + ": " +theProviderFilename);

			var name="";
			if (showPhonesFlag == "yes")	{
				name = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Dial-In Configuration","Phone");
				}
			if (name==null || name=="")	{
				var name = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Dial-In Configuration","SiteName");
				}

			if (theProviderFilename!=null && theProviderFilename!="" && name!=null && name!="")	{
				ISParray[j++]=new ISP(theProviderFilename,name);
				}
			}

		// un-comment the following line to sort the ISP array
		// Note: for large (more than a dozen or so) lists, this is AMAZINGLY slow
		// (unsorted data takes seconds; sorted data can take more than a minute)

//		ISParray.sort(ISPcompare);

		for (var x=0; x<ISParray.length; x++)	{
			var y = document.forms[0].providerlist.length;
			document.forms[0].providerlist.options[y] = new Option(ISParray[x].name,ISParray[x].theProviderFilename,false,false);
			document.forms[0].providerlist.options[y].selected = ((ISParray[x].name == parent.parent.globals.document.vars.providername.value) ? true:false);
			}

		var showNoneAboveFlag = parent.parent.globals.document.setupPlugin.GetNameValuePair(theAcctSetupFile,"Existing Acct Mode","ShowNoneAbove");
		if (showNoneAboveFlag != null && showNoneAboveFlag != "")	{
			showNoneAboveFlag = showNoneAboveFlag.toLowerCase();
			}
		if (showNoneAboveFlag != "no")	{
			x = document.forms[0].providerlist.options.length;
			document.forms[0].providerlist.options[x] = new Option("(None of the above)","",false,false);
			document.forms[0].providerlist.options[x].selected = false;
			}
		}	
}



// end hiding contents from old browsers  -->
