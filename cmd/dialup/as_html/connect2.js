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



function configureDialer()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var theFile = parent.parent.globals.getAcctSetupFilename(self);
	var intlFlag = parent.parent.globals.GetNameValuePair(theFile,"Mode Selection","IntlMode");
	intlFlag = intlFlag.toLowerCase();


	var theFolder = parent.parent.globals.getConfigFolder(self);
	theRegFile = theFolder + parent.parent.globals.document.vars.regServer.value;


	// platform check
	var thePlatform = new String(navigator.userAgent);
	var x=thePlatform.indexOf("(")+1;
	var y=thePlatform.indexOf(";",x+1);
	thePlatform=thePlatform.substring(x,y);


/*
	// determine name of scripting file
	var scriptEnabledFlag = parent.parent.globals.GetNameValuePair(theRegFile,"Script","ScriptEnabled");
	scriptEnabledFlag = scriptEnabledFlag.toLowerCase();
	var theScriptFile = "";
	if (scriptEnabledFlag == "yes")	{
		theScriptFile = parent.parent.globals.GetNameValuePair(theRegFile,"Script","ScriptFileName");
		if (theScriptFile != null && theScriptFile != "")	{
			theScriptFile = theFolder + theScriptFile;
			scriptEnabledFlag = "TRUE";
			}
		else	{
			theScriptFile="";
			scriptEnabledFlag = "FALSE";
			}
		}
	else	{
		scriptEnabledFlag = "FALSE";
		}
*/


	// determine outside line access string
	
	var outsideLineAccessStr = "";
	if (parent.parent.globals.document.vars.prefixData.value != "")	{
		outsideLineAccessStr = parent.parent.globals.document.vars.prefixData.value;
		x = outsideLineAccessStr.indexOf(",");
		if (x<0)	outsideLineAccessStr = outsideLineAccessStr + ",";
		}


	// build TAPI phone number

	if (intlFlag == "yes")	{
		var thePhone = parent.parent.globals.document.vars.accountPhoneNumber.value;
		var theCountry = "";
		var theCountryCode="";					// XXX
		var longDistanceAccess="";
		var dialAsLongDistanceFlag="FALSE";
		var	dialAreaCodeFlag="FALSE";
		var userAreaCode="";
		}
	else	{
		var thePhone = "(" + parent.parent.globals.document.vars.accountAreaCode.value + ") " + parent.parent.globals.document.vars.accountPhoneNumber.value;
		var theCountry = "USA";
		var theCountryCode="1";
		var longDistanceAccess="1";				// XXX
		var dialAsLongDistanceFlag="TRUE";
		var	dialAreaCodeFlag="TRUE";
		var userAreaCode=parent.parent.globals.document.vars.modemAreaCode.value;
		if (userAreaCode == parent.parent.globals.document.vars.accountAreaCode.value)	{
			dialAsLongDistanceFlag="FALSE";
			dialAreaCodeFlag = "FALSE";
			}

		}


	// determine new profile name (used for Account in dialer & profile name, save info default filename)

	var	newProfileName = parent.parent.globals.document.vars.accountName.value;
	if (newProfileName=="")	{
		newProfileName = parent.parent.globals.document.vars.first.value;
		if (parent.parent.globals.document.vars.last.value != "")	{
			newProfileName = newProfileName + " " + parent.parent.globals.document.vars.last.value;
			}
		}
	if (newProfileName!="")	newProfileName = newProfileName + "'s";
	if (parent.parent.globals.document.vars.providername.value != "")	{
		newProfileName = newProfileName + " " + parent.parent.globals.document.vars.providername.value;
		}
	newProfileName = newProfileName + " Account";
	if (newProfileName.length > 240)	newProfileName=newProfileName.substring(0,240);


	if (thePlatform == "Win16")	{
		if (newProfileName.length > 40)	newProfileName=newProfileName.substring(0,40);
		}

	
	// On WIN32 platforms, check if newProfileName contains any invalid characters, such as '/'
	// On Mac, disallow invalid characters such as ':'

	if ((thePlatform == "WinNT") || (thePlatform == "Win95")) {
		var x=0;
		x = newProfileName.indexOf('/');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf('/');
			}
		x = newProfileName.indexOf('\\');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf('\\');
			}
		x = newProfileName.indexOf(':');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf(':');
			}
		x = newProfileName.indexOf('\"');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf('\"');
			}
		x = newProfileName.indexOf('?');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf('?');
			}
		x = newProfileName.indexOf('<');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf('<');
			}
		x = newProfileName.indexOf('>');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf('>');
			}
		x = newProfileName.indexOf('|');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf('|');
			}
		x = newProfileName.indexOf('&');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(0, x) + newProfileName.substring(x+1, newProfileName.length);
			x = newProfileName.indexOf('&');
			}
		}
	else if (thePlatform == "Macintosh")	{
		var x=0;
		x = newProfileName.indexOf(':');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf(':');
			}
		}
	else if (thePlatform == "Win31") {
		var x=0;
		x = newProfileName.indexOf('(');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(0, x) + newProfileName.substring(x+1, newProfileName.length);
			x = newProfileName.indexOf('(');
			}
		x = newProfileName.indexOf(')');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(0, x) + newProfileName.substring(x+1, newProfileName.length);
			x = newProfileName.indexOf(')');
			}
		}


	// configure dialer

	dialerData = parent.parent.globals.document.setupPlugin.newStringArray(28);		// increment this # as new dialer strings are added
	dialerData[0]  = "FileName=" + theRegFile;
	dialerData[1]  = "AccountName=" + newProfileName;								// parent.parent.globals.document.vars.providername.value;
	dialerData[2]  = "ISPPhoneNum=" + thePhone;
	dialerData[3]  = "LoginName=" + parent.parent.globals.document.vars.accountName.value;
	dialerData[4]  = "Password=" + parent.parent.globals.document.vars.accountPassword.value;
	dialerData[5]  = "DNSAddress=" + parent.parent.globals.document.vars.primaryDNS.value;
	dialerData[6]  = "DNSAddress2=" + parent.parent.globals.document.vars.secondaryDNS.value;
	dialerData[7]  = "DomainName=" + parent.parent.globals.document.vars.domainName.value;
	dialerData[8]  = "IPAddress=" + parent.parent.globals.document.vars.ipAddress.value;
	dialerData[9]  = "IntlMode=" + ((intlFlag=="yes") ? "TRUE":"FALSE");
	dialerData[10] = "DialOnDemand=TRUE";
	dialerData[11] = "ModemName=" + parent.parent.globals.document.vars.modem.value;
	dialerData[12] = "ModemType=" + parent.parent.globals.document.setupPlugin.GetModemType(parent.parent.globals.document.vars.modem.value);
	dialerData[13] = "DialType=" + parent.parent.globals.document.vars.dialMethod.value;
	dialerData[14] = "OutsideLineAccess=" + outsideLineAccessStr;
	dialerData[15] = "DisableCallWaiting=" + ((parent.parent.globals.document.vars.cwData.value != "") ? "TRUE":"FALSE");
	dialerData[16] = "DisableCallWaitingCode=" + parent.parent.globals.document.vars.cwData.value;
	dialerData[17] = "UserAreaCode=" + userAreaCode;
	dialerData[18] = "CountryCode=" + theCountryCode;
	dialerData[19] = "LongDistanceAccess=" + longDistanceAccess;
	dialerData[20] = "DialAsLongDistance=" + dialAsLongDistanceFlag;
	dialerData[21] = "DialAreaCode=" + dialAreaCodeFlag;
	dialerData[22] = "ScriptEnabled=" + parent.parent.globals.document.vars.scriptEnabled.value;
	dialerData[23] = "ScriptFileName=" + parent.parent.globals.document.vars.scriptFile.value;
	dialerData[24] = "NeedsTTYWindow=" + (parent.parent.globals.document.vars.ttyWindow.checked ? "TRUE":"FALSE");									// XXX
	dialerData[25] = "Location=Home";
	dialerData[26] = "DisconnectTime=" + parent.parent.globals.GetNameValuePair(theFile,"Mode Selection","Dialer_Disconnect_After");
	dialerData[27] = "Path=Existing";


	// write out dialer data to Java Console

	if (parent.parent.globals.document.vars.debugMode.value.toLowerCase() == "yes")	{
		parent.parent.globals.debug("\nDialer data (ISP: '" + parent.parent.globals.document.vars.providername.value + "'): ");
		var numElements = dialerData.length;
		for (var x=0; x<numElements; x++)	{
			parent.parent.globals.debug("        " + x + ": " + dialerData[x]);
			}
		}

	parent.parent.globals.document.setupPlugin.DialerConfig(dialerData,false);


	// configure desktop (Windows)

	var fileName = parent.parent.globals.document.vars.providerFilename.value;
	var iconFilename = fileName.toUpperCase();
	if (iconFilename == "")	{
		iconFilename = theFolder + "DEFAULT.ICO";
		}
	else	{
		var x = iconFilename.lastIndexOf(".NCI");
		if (x>0)	{
			iconFilename = iconFilename.substring(0,x) + ".ICO";
			}
		else	{
			iconFilename = "";
			}
		}

	parent.parent.globals.document.setupPlugin.DesktopConfig(newProfileName, iconFilename, theFile);


	// set up Navigator preferences

	var userName = parent.parent.globals.document.vars.first.value;
	if (parent.parent.globals.document.vars.last.value != "")	{
		userName = userName + " " + parent.parent.globals.document.vars.last.value;
		}

	navigator.preference("network.hosts.smtp_server",		parent.parent.globals.document.vars.SMTP.value);
	navigator.preference("network.hosts.nntp_server",		parent.parent.globals.document.vars.NNTP.value);

	navigator.preference("network.hosts.pop_server",	parent.parent.globals.document.vars.mailServer.value);
	if (parent.parent.globals.document.vars.mailProtocol.value.toUpperCase() == "IMAP")	{
		navigator.preference("mail.server_type",			1);
		}
	else	{
		navigator.preference("mail.server_type",			0);
		}

	var mailID=""
	var mailAccount = parent.parent.globals.document.vars.emailName.value;
	x = mailAccount.indexOf("@");
	if (x>=0)	{
		mailID=mailAccount.substring(0,x);
		}
	else	{
		mailID=mailAccount;
		if (parent.parent.globals.document.vars.domainName.value != "")	{
			mailAccount = mailID + "@" + parent.parent.globals.document.vars.domainName.value;
			}
		}

	navigator.preference("mail.pop_name",					mailID);
	navigator.preference("mail.identity.organization",		parent.parent.globals.document.vars.company.value);
	navigator.preference("mail.identity.reply_to",			mailAccount);
	navigator.preference("mail.identity.username",			userName);
	navigator.preference("mail.identity.useremail",			mailAccount);

	navigator.preference("mail.remember_password",	false);
	navigator.preference("mail.pop_password",		"");
	var theMailPassword=parent.parent.globals.document.vars.emailPassword.value;
	if (theMailPassword != null && theMailPassword != "")	{
		theMailPassword = parent.parent.globals.document.setupPlugin.EncryptString(theMailPassword);
		if (theMailPassword != null && theMailPassword != "")	{
			navigator.preference("mail.remember_password",	true);
			navigator.preference("mail.pop_password",		theMailPassword);
			}
		}

	navigator.preference("editor.author",					userName);
	navigator.preference("editor.publish_username",			parent.parent.globals.document.vars.accountName.value);
	navigator.preference("editor.publish_location",			parent.parent.globals.document.vars.publishURL.value);
	navigator.preference("editor.publish_browse_location",	parent.parent.globals.document.vars.viewURL.value);

	navigator.preference("editor.publish_password",			"");
	navigator.preference("editor.publish_save_password",	false);
	var thePublishPassword=parent.parent.globals.document.vars.publishPassword.value;
	if (thePublishPassword != "")	{
		thePublishPassword = parent.parent.globals.document.setupPlugin.EncryptString(thePublishPassword);
		if (thePublishPassword != null && thePublishPassword != "")	{
			navigator.preference("editor.publish_password",			thePublishPassword);
			navigator.preference("editor.publish_save_password",	true);
			}
		}


	navigator.preference("browser.mac.use_internet_config", false);


	var profileDir = parent.parent.globals.document.setupPlugin.GetCurrentProfileDirectory();
	if (profileDir != null && profileDir != "")	{

		// write MUC Configuration file

		var thePlatform = new String(navigator.userAgent);
		var x=thePlatform.indexOf("(")+1;
		var y=thePlatform.indexOf(";",x+1);
		thePlatform=thePlatform.substring(x,y);
	
		var configFile="";
		if (thePlatform == "Macintosh")	{				// Macintosh support
			configFile = profileDir + "Configuration";
			}
		else	{										// Windows support
			configFile = profileDir + "CONFIG.INI";
			}
		
		parent.parent.globals.document.setupPlugin.SetNameValuePair(configFile,"Account", "Account", newProfileName);		// parent.parent.globals.document.vars.providername.value);
		parent.parent.globals.document.setupPlugin.SetNameValuePair(configFile,"Modem", "Modem", parent.parent.globals.document.vars.modem.value);
		parent.parent.globals.document.setupPlugin.SetNameValuePair(configFile,"Location", "Location", "Home");
		

		// write out default Bookmark file (if one doesn't exist yet)
		
		var currentBookmarkFilename="";
		if (thePlatform == "Macintosh")	{				// Macintosh support
			currentBookmarkFilename = profileDir + "Bookmarks.html";
			}
		else	{										// Windows support
			currentBookmarkFilename = profileDir + "BOOKMARK.HTM";
			}

		var bookmarkData = parent.parent.globals.document.setupPlugin.GetNameValuePair(currentBookmarkFilename,null,null);
		if ((bookmarkData == null) || (bookmarkData == "") || (bookmarkData.indexOf("HREF")<0))	{
			var defaultBookmarkFilename = parent.parent.globals.getConfigFolder(self) + "bookmark.htm";
			bookmarkData = parent.parent.globals.document.setupPlugin.GetNameValuePair(defaultBookmarkFilename,null,null);
			if (bookmarkData != null && bookmarkData != "")	{
				parent.parent.globals.document.setupPlugin.SaveTextToFile(currentBookmarkFilename,bookmarkData,false);
				}
			}

		// copy profile lock file (if one is specified in selected .NCI file)
		
		theProviderFilename = parent.parent.globals.document.vars.providerFilename.value;
		if (theProviderFilename != "")	{
			var configLockFile = parent.parent.globals.document.setupPlugin.GetNameValuePair(theProviderFilename,"Configuration","ConfigurationFileName");
			if (configLockFile != null && configLockFile != "")	{

				// read in .CFG file (from Config folder)

				configLockFile = theFolder + configLockFile;
				var cfgData = parent.parent.globals.document.setupPlugin.ReadFile(configLockFile);
				if (cfgData != null && cfgData != "")	{

					// write out PROFILE.CFG file (inside of current profile directory)

					var profileLockfilename = profileDir + "PROFILE.CFG";
					parent.parent.globals.document.setupPlugin.WriteFile(profileLockfilename,cfgData);
					}
				}
			}
		}


	// rename profile

	if (thePlatform == "Macintosh") {
		if (newProfileName.length > 31)	newProfileName=newProfileName.substring(0,31);
		}

	parent.parent.globals.document.setupPlugin.SetCurrentProfileName(newProfileName);
	if (parent.parent.globals.document.vars.debugMode.value.toLowerCase() == "yes")	{
		parent.parent.globals.debug("\nSetting profile name: " + newProfileName);
		}
}



function showWindowBars()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	// check browser version
	var theAgent=navigator.userAgent;
	var x=theAgent.indexOf("/");
	if (x>=0)	{
		theVersion=theAgent.substring(x+1,theAgent.length);
		x=theVersion.indexOf(".");
		if (x>0)	{
			theVersion=theVersion.substring(0,x);
			}			
		if (parseInt(theVersion)>=4)	{
			top.statusbar.visible=true;
			top.scrollbars.visible=true;
			top.toolbar.visible=true;
			top.menubar.visible=true;
			top.locationbar.visible=true;
			top.personalbar.visible=true;
			}
		}
	parent.parent.globals.document.setupPlugin.SetKiosk(false);
}



function setLocation(theURL)
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");
	
	var theWindow=window.open(theURL,"__BLANK","toolbar=yes,location=yes,directories=yes,status=yes,menubar=yes,scrollbars=yes,resizable=yes");

//	parent.parent.location.replace(theURL);										// jumping to the URL
	top.close();
}



function go(msg)
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (msg=="Connect Now")	{

		if (parent.parent.globals.document.vars.editMode.value != "yes")	{
			configureDialer();

			if (parent.parent.globals.document.setupPlugin.NeedReboot() == true)	{
				parent.parent.globals.forceReboot("connect2.htm");				// XXX hardcode in name of next screen???
				return(false);
				}

			if (parent.parent.globals.document.setupPlugin.DialerConnect() == false)	{
				window.location.replace("error.htm");							// XXX hardcode in name of next screen???
				return(false);
				}
	
//			showWindowBars();
	
			var theFile = parent.parent.globals.getAcctSetupFilename(self);
			var theURL = parent.parent.globals.GetNameValuePair(theFile,"Existing Acct Mode","RegPodURL");
			if (theURL == null || theURL == "")	{
				theURL = "http://home.netscape.com/";
				}
			setTimeout("setLocation(\'" + theURL + "\')", 1000);
			}
		else	{
			alert("You cannot connect while in edit mode.");
			return(false);
			}
		}
	else if (msg == "error.htm")	{
		if (parent.parent.globals.document.vars.editMode.value == "yes")	{	// only do this if editMode is false
				return (confirm("Since you are in edit mode, would you like to edit the error screen that users will see if the connection fails?"));
				}
		else	{
			return(false);
			}
		}
	else if ((msg=="Later") && (parent.parent.globals.document.vars.editMode.value != "yes"))	{	
		configureDialer();
		//showWindowBars();
		return(true);
		}
	else if (msg=="Back")	{
		history.back();
		}
	return(false);
}



function checkData()
{
	return(true);
}



function doGo()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");
	parent.controls.go("Next");
}



function loadData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (parent.parent.globals.document.vars.tryAgain.value == "yes")	{
		parent.parent.globals.document.vars.tryAgain.value = "no";
		setTimeout("doGo()",1);
		}

	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
}



// end hiding contents from old browsers  -->
