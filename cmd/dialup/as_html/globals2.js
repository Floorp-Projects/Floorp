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



compromisePrincipals();



function configureNewAccount()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var theFile = getAcctSetupFilename(self);
	var intlFlag = GetNameValuePair(theFile,"Mode Selection","IntlMode");
	intlFlag = intlFlag.toLowerCase();


	var theScript = findVariable("LOGIN_SCRIPT");


	// determine outside line access string
	
	var outsideLineAccessStr = "";
	if (document.vars.prefixData.value != "")	{
		outsideLineAccessStr = document.vars.prefixData.value;
		x = outsideLineAccessStr.indexOf(",");
		if (x<0)	outsideLineAccessStr = outsideLineAccessStr + ",";
		}

	var dialAsLongDistance = findVariable("DIAL_AS_LONG_DISTANCE");
	if (dialAsLongDistance != null && dialAsLongDistance!="")	{
		var dialAsLongDistanceFlag = (dialAsLongDistance=="NO") ? "FALSE":"TRUE";
		var dialAreaCode = findVariable("DIAL_AREA_CODE");
		var dialAreaCodeFlag = "FALSE";
		if (dialAreaCode != null && dialAreaCode != "")	{
			dialAreaCodeFlag = (dialAreaCode == "NO") ? "FALSE":"TRUE";
			}
		}
	else if (intlFlag == "yes")	{
		var dialAsLongDistanceFlag = "FALSE";
		var dialAreaCodeFlag = "FALSE";
		}
	else	{
		var dialAsLongDistanceFlag = "TRUE";
		var dialAreaCodeFlag = "TRUE";

		var ispAreaCode="";
		var ispPhoneNum = findVariable("PHONE_NUM");
		if (ispPhoneNum != null && ispPhoneNum!="")	{
			var x=ispPhoneNum.indexOf("(");
			if (x>=0)	{
				var y=ispPhoneNum.indexOf(")",x+1);
				ispAreaCode=ispPhoneNum.substring(x+1,y);
				}
			}
		if (ispAreaCode == document.vars.modemAreaCode.value)	{
			dialAsLongDistanceFlag="FALSE";
			dialAreaCodeFlag = "FALSE";
			}
		}


	// determine new profile name (used for Account in dialer & profile name)

	var	newProfileName = findVariable("LOGIN");
	if (newProfileName=="")	{
		newProfileName = document.vars.first.value;
		if (document.vars.last.value != "")	{
			newProfileName = newProfileName + " " + document.vars.last.value;
			}
		}
	if (newProfileName!="")	newProfileName = newProfileName + "'s";
	if (findVariable("SITE_NAME") != "")	{
		newProfileName = newProfileName + " " + findVariable("SITE_NAME");
		}
	newProfileName = newProfileName + " Account";
	if (newProfileName.length > 240)	newProfileName=newProfileName.substring(0,240);


	// platform check

	var thePlatform = new String(navigator.userAgent);
	var x=thePlatform.indexOf("(")+1;
	var y=thePlatform.indexOf(";",x+1);
	thePlatform=thePlatform.substring(x,y);

	if (thePlatform == "Win16")	{
		if (newProfileName.length > 40)	newProfileName=newProfileName.substring(0,40);
		}

	
	// On Win32 platforms, check if newProfileName contains any invalid characters, such as '/'
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
	else if (thePlatform == "Win16") {
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


	// configure dialer for new account 

	dialerData = document.setupPlugin.newStringArray(24);		// increment this # as new dialer strings are added
	dialerData[0]  = "FileName=" + findVariable("SITE_FILE");
	dialerData[1]  = "AccountName=" + newProfileName;
	dialerData[2]  = "ISPPhoneNum=" + findVariable("PHONE_NUM");
	dialerData[3]  = "LoginName=" + findVariable("LOGIN");
	dialerData[4]  = "Password=" + findVariable("PASSWORD");
	dialerData[5]  = "DNSAddress=" + findVariable("DNS_ADDR");
	dialerData[6]  = "DNSAddress2=" + findVariable("DNS_ADDR_2");
	dialerData[7]  = "DomainName=" + findVariable("DOMAIN_NAME");
	dialerData[8]  = "IntlMode=" + ((intlFlag=="yes") ? "TRUE":"FALSE");
	dialerData[9]  = "DialOnDemand=TRUE";
	dialerData[10] = "ModemName=" + document.vars.modem.value;
	dialerData[11] = "ModemType=" + document.setupPlugin.GetModemType(document.vars.modem.value);
	dialerData[12] = "DialType=" + document.vars.dialMethod.value;
	dialerData[13] = "OutsideLineAccess=" + outsideLineAccessStr;
	dialerData[14] = "DisableCallWaiting=" + ((parent.parent.globals.document.vars.cwData.value != "") ? "TRUE":"FALSE");
	dialerData[15] = "DisableCallWaitingCode=" + parent.parent.globals.document.vars.cwData.value;
	dialerData[16] = "DialAsLongDistance=" + dialAsLongDistanceFlag;
	dialerData[17] = "DialAreaCode=" + dialAreaCodeFlag;
	dialerData[18] = "ScriptEnabled=" + ((theScript==null || theScript=="") ? "FALSE":"TRUE");
	dialerData[19] = "Script=" + theScript;
	dialerData[20] = "AutoSend=" + findVariable("AUTO_SEND");
	dialerData[21] = "Location=Home";
	dialerData[22] = "DisconnectTime=" + GetNameValuePair(theFile,"Mode Selection","Dialer_Disconnect_After");
	dialerData[23] = "Path=New";


	// write out dialer data to Java Console

	debug("\nNew Account for ISP: " + findVariable("SITE_NAME"));
	var numElements = dialerData.length;
	for (var x=0; x<numElements; x++)	{
		debug("        " + x + ": " + dialerData[x]);
		}

	parent.parent.globals.document.setupPlugin.DialerConfig(dialerData,false);


	// configure desktop (Windows)

	parent.parent.globals.document.setupPlugin.DesktopConfig(newProfileName, null, theFile);


	// set up Navigator preferences

	var userName = document.vars.first.value;
	if (document.vars.last.value != "")	{
		userName = userName + " " + document.vars.last.value;
		}

	navigator.preference("network.hosts.smtp_server",		findVariable("SMTP_HOST"));
	navigator.preference("network.hosts.nntp_server",		findVariable("NNTP_HOST"));

	var mailID=""
	var mailAccount = findVariable("IMAP_HOST");
	if (mailAccount == "")	{
		mailAccount = findVariable("POP_SERVER");
		}
	var mailServer="";

	x = mailAccount.indexOf("@");
	if (x>=0)	{
		mailID=mailAccount.substring(0,x);
		mailServer=mailAccount.substring(x+1,mailAccount.length);
		}
	else	{
		mailID=mailAccount;
		mailServer=findVariable("DOMAIN_NAME");
		if (mailServer != "")	{
			mailAccount = mailID + "@" + mailServer;
			}
		}

	navigator.preference("network.hosts.pop_server",	mailServer);
	if (findVariable("IMAP_HOST") != "")	{
		navigator.preference("mail.server_type",			1);
		navigator.preference("mail.imap.server_sub_directory",findVariable("IMAP_SERVERPATH"));
		}
	else	{
		navigator.preference("mail.server_type",			0);
		}

	navigator.preference("mail.pop_name",					mailID);
	navigator.preference("mail.identity.organization",		document.vars.company.value);
	navigator.preference("mail.identity.reply_to",			findVariable("EMAIL_ADDR"));
	navigator.preference("mail.identity.username",			userName);
	navigator.preference("mail.identity.useremail",			mailAccount);

	navigator.preference("mail.remember_password",	false);
	navigator.preference("mail.pop_password",		"");
	var theMailPassword=findVariable("POP_PASSWORD");
	if (theMailPassword != null && theMailPassword != "")	{
		theMailPassword = parent.parent.globals.document.setupPlugin.EncryptString(theMailPassword);
		if (theMailPassword != null && theMailPassword != "")	{
			navigator.preference("mail.remember_password",	true);
			navigator.preference("mail.pop_password",		theMailPassword);
			}
		}

	navigator.preference("editor.author",					userName);
	navigator.preference("editor.publish_username",			mailID);
	var pushURL = findVariable("PUBLISH_PUSH_URL");
	navigator.preference("editor.publish_location",			pushURL);
	navigator.preference("editor.publish_browse_location",	findVariable("PUBLISH_VIEW_URL"));

	navigator.preference("editor.publish_password",			"");
	navigator.preference("editor.publish_save_password",	false);
	if (pushURL != null && pushURL != "")	{
		var thePublishPassword=findVariable("PASSWORD");
		if (thePublishPassword != "")	{
			thePublishPassword = parent.parent.globals.document.setupPlugin.EncryptString(thePublishPassword);
			if (thePublishPassword != null && thePublishPassword != "")	{
				navigator.preference("editor.publish_save_password",	true);
				navigator.preference("editor.publish_password",			thePublishPassword);
				}
			}
		}

	if (findVariable("HOME_URL") != "")	{
		navigator.preference("browser.startup.page",		1);							// 0 blank, 1 homepage, 2 last visited
		navigator.preference("browser.startup.homepage",	findVariable("HOME_URL"));
		}


	// look for LDAP data

	var ldapNum = navigator.preference("ldap_1.number_of_directories");
	if (ldapNum == null || ldapNum == "")	{
		// if # of directories isn't defined, count any existing LDAP entries
		ldapNum=1;
		while(true)	{
			var ldapEntry = navigator.preference("ldap_1.directory" + ldapNum + ".filename");
			if (ldapEntry == null || ldapEntry == "")	break;
			ldapNum = ldapNum + 1;
			}
		}
	ldapNum = 1 + ldapNum;

	var ldapEntriesAddedFlag=false;
	var ldapIndex=1;
	while(true)	{
		var ldapURL = findVariable("LDAP_HOST_" + ldapIndex);
		if (ldapURL == null || ldapURL == "")	break;
		
		var secureLDAP = false;
		var	searchBase = "";
		var portNum = 389;

		if (ldapURL.indexOf("ldaps://")==0)	{					// LDAP over SSL
			secureLDAP = true;
			portNum = 636;
			ldapURL = ldapURL.substring(8,ldapURL.length);
			}
		else if (ldapURL.indexOf("ldap://")==0)	{
			ldapURL = ldapURL.substring(7,ldapURL.length);
			}
		var x = ldapURL.indexOf("/");							// find any search base
		if (x>0)	{
			searchBase = ldapURL.substring(x+1,ldapURL.length);
			ldapURL = ldapURL.substring(0,x);
			}
		x = ldapURL.indexOf(":");								// find any port number
		if (x>0)	{
			portNumString = ldapURL.substring(x+1,ldapURL.length);
			ldapURL = ldapURL.substring(0,x);
			if (portNumString != "")	{
				portNum = parseInt(portNumString);
				}
			}

		var ldapDesc = findVariable("LDAP_DESC_" + ldapIndex);
		if (ldapDesc == "")	{
			ldapDesc = ldapURL;
			}

		ldapEntriesAddedFlag = true;
		navigator.preference("ldap_1.directory" + ldapNum + ".filename", "");
		navigator.preference("ldap_1.directory" + ldapNum + ".description", ldapDesc);
		navigator.preference("ldap_1.directory" + ldapNum + ".serverName", ldapURL);
		navigator.preference("ldap_1.directory" + ldapNum + ".port", portNum);
		navigator.preference("ldap_1.directory" + ldapNum + ".isSecure", secureLDAP);
		navigator.preference("ldap_1.directory" + ldapNum + ".searchBase", searchBase);
		navigator.preference("ldap_1.directory" + ldapNum + ".searchString", "");
		navigator.preference("ldap_1.directory" + ldapNum + ".dirType", 0);
		navigator.preference("ldap_1.directory" + ldapNum + ".isOffline", false);
		navigator.preference("ldap_1.directory" + ldapNum + ".savePassword", false);

		if (document.vars.debugMode.value.toLowerCase() == "yes")	{
			debug("\tLDAP #" + ldapNum + " Desc: " + ldapDesc);
			debug("\tLDAP #" + ldapNum + " serverName: " + ldapURL);
			debug("\tLDAP #" + ldapNum + " port: " + portNum);
			debug("\tLDAP #" + ldapNum + " isSecure: " + secureLDAP);
			debug("\tLDAP #" + ldapNum + " searchBase: " + searchBase);
			}
		
		ldapNum = ldapNum + 1;
		ldapIndex = ldapIndex + 1;
		}

	if (ldapEntriesAddedFlag == true)	{
		navigator.preference("ldap_1.number_of_directories", ldapNum);
		}


	// on Mac, prevent Internet Config from overriding new settings

	navigator.preference("browser.mac.use_internet_config", false);


	var profileDir = document.setupPlugin.GetCurrentProfileDirectory();
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
		
		document.setupPlugin.SetNameValuePair(configFile,"Account", "Account", newProfileName);			// findVariable("SITE_NAME")
		document.setupPlugin.SetNameValuePair(configFile,"Modem", "Modem", document.vars.modem.value);
		document.setupPlugin.SetNameValuePair(configFile,"Location", "Location", "Home");


		// write out Reggie bookmark file (if one was sent)
		
		var currentBookmarkFilename="";
		var CRLF="";
		if (thePlatform == "Macintosh")	{				// Macintosh support
			currentBookmarkFilename = profileDir + "Bookmarks.html";
			CRLF = "\r";
			}
		else	{										// Windows support
			currentBookmarkFilename = profileDir + "BOOKMARK.HTM";
			CRLF = "\r\n";
			}


		// Mac only: on clean install, core Communicator doesn't copy over Bookmarks.html file from Defaults folder
		// so grab a copy from our Config folder

		if (thePlatform == "Macintosh")	{
			var theActiveProfileName = document.setupPlugin.GetCurrentProfileName();
			if (theActiveProfileName != null)	{
				theActiveProfileName = theActiveProfileName.toUpperCase();
				if (theActiveProfileName == '911' || theActiveProfileName == 'USER1')	{
					var theDefaultBookmarkFilename = parent.parent.globals.getConfigFolder(self) + "Bookmarks.html";
					var theBookmarkData = parent.parent.globals.document.setupPlugin.GetNameValuePair(theDefaultBookmarkFilename,null,null);
					if (theBookmarkData == null || theBookmarkData == "")	{
						theDefaultBookmarkFilename = parent.parent.globals.getConfigFolder(self) + "bookmark.htm";
						theBookmarkData = parent.parent.globals.document.setupPlugin.GetNameValuePair(theDefaultBookmarkFilename,null,null);
						}
					if (theBookmarkData != null && theBookmarkData != "")	{
						parent.parent.globals.document.setupPlugin.SaveTextToFile(currentBookmarkFilename,theBookmarkData,false);
						}
					}
				}
			}


		var bookmarkData = "" + document.setupPlugin.GetNameValuePair(currentBookmarkFilename,null,null);
		if (bookmarkData != "")	{
			if (bookmarkData.indexOf("<!DOCTYPE NETSCAPE-Bookmark-file-1>") ==0)	{					// check for valid bookmark file header

				// build new bookmark title

				var titleStr = "Bookmarks";
				var name = "";
				if (parent.parent.globals.document.vars.first.value != "" && parent.parent.globals.document.vars.last.value != "")	{
					name = parent.parent.globals.document.vars.first.value + " " + parent.parent.globals.document.vars.last.value;
					}
				else	{
					name = findVariable("LOGIN");
					}
				if (name != "")	titleStr = titleStr + " for " + name;

				// change TITLE section

				var startTitleindex=bookmarkData.indexOf("<TITLE>");
				var endTitleindex=bookmarkData.indexOf("</TITLE>");
				if (startTitleindex>0 && endTitleindex>0)	{
					startTitleindex = startTitleindex + "<TITLE>".length;
					var bookmarkDataLen = bookmarkData.length;
					bookmarkData = bookmarkData.substring(0,startTitleindex) + titleStr + bookmarkData.substring(endTitleindex,bookmarkDataLen);
					}

				// change H1 section

				var startTitleindex=bookmarkData.indexOf("<H1>");
				var endTitleindex=bookmarkData.indexOf("</H1>");
				if (startTitleindex>0 && endTitleindex>0)	{
					startTitleindex = startTitleindex + "<H1>".length;
					var bookmarkDataLen = bookmarkData.length;
					bookmarkData = bookmarkData.substring(0,startTitleindex) + titleStr + bookmarkData.substring(endTitleindex,bookmarkDataLen);
					}
				}
			}

		var regBookmarkData = document.vars.regBookmark.value;
		if (regBookmarkData != null && regBookmarkData != "")	{
			if (regBookmarkData.indexOf("<!DOCTYPE NETSCAPE-Bookmark-file-1>") == 0 )	{				// check for valid bookmark file header
				var cleanFlag = false;
				var activeProfileName = document.setupPlugin.GetCurrentProfileName();
				if (activeProfileName != null)	{
					activeProfileName = activeProfileName.toUpperCase();
					if (activeProfileName == '911' || activeProfileName == 'USER1')	{
						cleanFlag=true;
						}
					}

				if (cleanFlag == true)	{																// if magic profile, write out entire new bookmark file
					bookmarkData = regBookmarkData;
					}
				else	{																				// else append onto end of bookmark file
					var headerStr = "<DL><p>" + CRLF;
					var startDLindex=regBookmarkData.indexOf(headerStr);
					if (startDLindex>0)	{
						startDLindex = startDLindex + headerStr.length;
						}
					var lastDLindex = regBookmarkData.lastIndexOf("</DL>");
					if (startDLindex>0 && startDLindex>0)	{											// remove bookmark header/footer data
						var newBookmarkData = regBookmarkData.substring(startDLindex,lastDLindex);
						if (newBookmarkData != "")	{
							if (bookmarkData.indexOf(newBookmarkData) <0)	{
								var startDLindex=bookmarkData.indexOf(headerStr);
								if (startDLindex>0)	{
									startDLindex = startDLindex + headerStr.length;
									}
								var lastDLindex = bookmarkData.lastIndexOf("</DL>");
								if (startDLindex>0 && startDLindex>0)	{								// merge into current bookmark file
									var bookmarkDataLen = bookmarkData.length;
									// append new bookmark data to bookmark file
									bookmarkData = bookmarkData.substring(0,lastDLindex) + newBookmarkData + bookmarkData.substring(lastDLindex,bookmarkDataLen);
									}
								}
							}
						}
					}
				}
			else	{																					// if invalid bookmark header, discard
				regBookmarkData = "";
				}
			}

		if (thePlatform == "Win16")	{
			if (bookmarkData.length >= 16000)	{
				bookmarkData = "";
				}
			}

		if (bookmarkData != "")	{
			document.setupPlugin.SaveTextToFile(currentBookmarkFilename,bookmarkData,false);
			}


		// append ISP bookmark (if Reggie sends it down) to profile's bookmark file

		var ISPurl = findVariable("ISP_URL");
		if (ISPurl != null && ISPurl != "")	{
			bookmarkData = "" + document.setupPlugin.GetNameValuePair(currentBookmarkFilename, null, null);
			if (bookmarkData != null && bookmarkData != "")	{
				var lastDLindex = bookmarkData.lastIndexOf("</DL>");
				if (lastDLindex >= 0)	{
					var newData = bookmarkData.substring(0,lastDLindex);
					newData = newData + "\t<DT><A HREF=\"" + ISPurl + "\">" + findVariable("SITE_NAME") + "</A>" + CRLF;
					newData = newData + bookmarkData.substring(lastDLindex,bookmarkData.length);

					if (thePlatform == "Win16")	{
						if (newData.length >= 16000)	{
							newData = "";
							}
						}
					if (newData != "")	{
						document.setupPlugin.SaveTextToFile(currentBookmarkFilename,newData,false);
						}
					}
				}
			}

		}


	// rename profile
	
	if (thePlatform == "Macintosh") {
		if (newProfileName.length > 31)	newProfileName=newProfileName.substring(0,31);
		}

	document.setupPlugin.SetCurrentProfileName(newProfileName);
	debug("\nSetting profile name: " + newProfileName);

	
	// set the default path to now be the existing path
	
	document.vars.path.value = "Existing Path";
}



function saveAccountInfo(promptFlag)
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var thePlatform = new String(navigator.userAgent);
	var x=thePlatform.indexOf("(")+1;
	var y=thePlatform.indexOf(";",x+1);
	thePlatform=thePlatform.substring(x,y);

	var CRLF;
	if (thePlatform == "Macintosh")	{
		CRLF = "\r";
		}
	else	{
		CRLF = "\r\n";
		}

	// Determine the current date

	var theFile = parent.parent.globals.getAcctSetupFilename(self);
	var savePasswordFlag = parent.parent.globals.GetNameValuePair(theFile,"New Acct Mode","SavePasswords");

	var now = new Date();
	theDate = now.getMonth()+1 + "/" + now.getDate() + "/" + (now.getYear()+1900);
	theDate += "  ";
	var theHour = now.getHours();
	var theString = " AM";
	if (theHour >=12)	{
		theHour = theHour-12;
		theString = " PM";
		}
	var theMinute = now.getMinutes();
	if (theMinute < 10)	{
		theMinute="0" + theMinute;
		}
	theDate += theHour + ":" + theMinute + theString;

	// Mangle the POP/IMAP server

	var mailProtocol = "IMAP";
	var popIMAPServer = findVariable("IMAP_HOST");
	if (popIMAPServer == "")	{
		mailProtocol = "POP";
		popIMAPServer = findVariable("POP_SERVER");
		}
	var atLocation = popIMAPServer.indexOf("@");
	if (atLocation>=0)	{
		popIMAPServer = popIMAPServer.substring(atLocation+1);
		}

	// Create the output string to save

	var output = "Your Account Information                  " + theDate + CRLF;
	   output += "______________________________________________________________" + CRLF + CRLF;
	   output += "Name:                                     " + parent.parent.globals.document.vars.first.value + " " + parent.parent.globals.document.vars.last.value + CRLF;
	   output += "Provider:                                 " + findVariable("SITE_NAME") + CRLF + CRLF;

	   output += "Dialup access number:                     " + findVariable("PHONE_NUM") + CRLF + CRLF;

	   output += "Login name:                               " + findVariable("LOGIN") + CRLF;

	if (savePasswordFlag == "yes")	{
	   output += "Login password:                           " + findVariable("PASSWORD") + CRLF + CRLF;
		}

	   output += "Email address:                            " + findVariable("EMAIL_ADDR") + CRLF;

	if (savePasswordFlag == "yes")	{
	   output += "Email password:                           " + findVariable("POP_PASSWORD") + CRLF + CRLF;
		}

	   output += "SMTP server:                              " + findVariable("SMTP_HOST") + CRLF;
	   
	if (mailProtocol == "IMAP")	{
	   output += "IMAP server:                              " + popIMAPServer + CRLF;
	   
	   var imapDir = findVariable("IMAP_SERVERPATH");
	   if (imapDir != "")	{
	   output += "IMAP server mailbox path:                 " + imapDir + CRLF;
			}
		}
	else	{
	   output += "POP server:                               " + popIMAPServer + CRLF;
		}
	   output += "News (NNTP) server:                       " + findVariable("NNTP_HOST") + CRLF + CRLF;

	   output += "Domain name:                              " + findVariable("DOMAIN_NAME") + CRLF;
	   output += "Primary DNS server:                       " + findVariable("DNS_ADDR") + CRLF;
	   output += "Secondary DNS server:                     " + findVariable("DNS_ADDR_2") + CRLF + CRLF;

	   var viewURL = findVariable("PUBLISH_VIEW_URL");
	   if (viewURL != "")	{
	   output += "Publishing View URL:                      " + viewURL + CRLF;
			}
	   var pushURL = findVariable("PUBLISH_PUSH_URL");
	   if (pushURL != "")	{
	   output += "Publishing Push URL:                      " + pushURL + CRLF;
			}

	   output += CRLF;

	   output += "Modem:                                    " + parent.parent.globals.document.vars.modem.value + CRLF + CRLF;

	   output += "Other information:" + CRLF + CRLF;
	   output += "Provider's technical support number:      " + findVariable("ISP_SUPPORT") + CRLF;


	// determine new profile name (used for Account in dialer & profile name, save info default filename)

	var	newProfileName = findVariable("LOGIN");
	if (newProfileName=="")	{
		newProfileName = document.vars.first.value;
		if (document.vars.last.value != "")	{
			newProfileName = newProfileName + " " + document.vars.last.value;
			}
		}
	if (newProfileName!="")	newProfileName = newProfileName + "'s";
	if (findVariable("SITE_NAME") != "")	{
		newProfileName = newProfileName + " " + findVariable("SITE_NAME");
		}
	newProfileName = newProfileName + " Account Info";

	if (thePlatform == "Macintosh") {
		if (newProfileName.length > 31)	newProfileName=newProfileName.substring(0,31);
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
		}
	else if (thePlatform == "Macintosh")	{
		var x=0;
		x = newProfileName.indexOf(':');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(x+1, newProfileName.length);			
			x = newProfileName.indexOf(':');
			}
		}
	else if (thePlatform == "Win16") {
		var x=0;
		x = newProfileName.indexOf('(');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(0, x-1) + newProfileName.substring(x+1, newProfileName.length);
			x = newProfileName.indexOf('(');
			}
		x = newProfileName.indexOf(')');
		while ((x >= 0) && (newProfileName.length != 0)) {
			newProfileName = newProfileName.substring(0, x-1) + newProfileName.substring(x+1, newProfileName.length);
			x = newProfileName.indexOf(')');
			}
		}


	// create the default filename to save output to
/*
	var defaultFilename = findVariable("SITE_FILE");
	if (defaultFilename != "")	defaultFilename=defaultFilename + " ";
	defaultFilename = defaultFilename + "Account Info";
*/

	var savedFlag=false;
	if (promptFlag==false)	{
		var profileDir = document.setupPlugin.GetCurrentProfileDirectory();
		if (profileDir != null && profileDir != "")	{
			if (thePlatform == "Macintosh")	{
				newProfileName = profileDir + "Account Info";
				}
			else	{
				newProfileName = profileDir + "ACCTINFO.TXT";
				}
			savedFlag = parent.parent.globals.document.setupPlugin.SaveTextToFile(newProfileName,output,false);		// defaultFilename
			}
		}
	else	{
		savedFlag = parent.parent.globals.document.setupPlugin.SaveTextToFile(newProfileName,output,true);		// defaultFilename
		}
	return(savedFlag);
}



// end hiding contents from old browsers  -->
