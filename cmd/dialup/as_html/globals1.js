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
enableExternalCapture(); 												// This requires UniversalBrowserWrite access
parent.captureEvents( Event.MOUSEDOWN | Event.MOUSEUP | Event.DRAGDROP | Event.DBLCLICK );
parent.onmousedown = cancelEvent;
parent.onmouseup = cancelEvent;
parent.ondragdrop = cancelEvent;
parent.ondblclick = cancelEvent;

var oneStepSemaphore = false;
var selectedISP = null;

function cancelEvent( e )
{
	var retVal = false;

	if ( ( e.which < 2 ) && ( e.type != "dragdrop" ) && ( e.type != "dblclick" ) )
		retVal = routeEvent( e );

	return retVal;
}



function debug( theString )
{
	java.lang.System.out.println( theString );
//	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );
//
//	if ( document.vars.debugMode.value.toLowerCase() == "yes" )
//		document.setupPlugin.debug( theString );
}

function GetNameValuePair( file, section, variable )
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	var value = parent.globals.document.setupPlugin.GetNameValuePair( file, section, variable );
	if ( value == null )
		value = "";
	return value;
}

function SetNameValuePair( file, section, variable, data )
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	parent.globals.document.setupPlugin.SetNameValuePair( file, section, variable, data );
	//debug( "\tSetNameValuePair: [" + section + "] " + variable + "=" + data );
}


// * returns a string representing the path to the folder named "Config" inside the folder "Data" that
//	is installed in the Communicator's home directory
function getConfigFolder( object )
{
	var	pathName;
	pathName = getFolder( object ) + "Config" + "\\";

	//debug( "getConfigFolder: " + pathName );

	return pathName;
}

// * returns a string representing the path to the file "ACCTSET.INI" inside the folder
//	returned from getConfigFolder
function getAcctSetupFilename( object )
{
	var file;
	file = getConfigFolder( object ) + "ACCTSET.INI";

	//debug( "getAcctSetupFilename: " + file );
	return file;
}

function getPlatform()
{
	var platform = new String( navigator.userAgent );
	var x = platform.indexOf( "(" ) + 1;
	var y = platform.indexOf( ";", x + 1 );
	platform = platform.substring( x, y );
	return platform;
}
	
// * returns a canoncial path to the folder containing the document representing the current contents of "window"
function getFolder( window )
{
	platform = getPlatform();

	if ( platform == "Macintosh" )
	{				// Macintosh support
		var path = unescape( window.location.pathname );
		if ( ( x = path.lastIndexOf( "/" ) ) > 0 )
			path = path.substring( 0, x + 1 );

		//var fileArray = path.split( "/" );
		//var newpath = fileArray.join( ":" ) + ":";

		//if ( newpath.charAt( 0 ) == ':' )
		//	newpath = newpath.substring( 1, newpath.length );

		newpath = path;
	}
	else	
	{										// Windows support

		// note: JavaScript returns path with '/' instead of '\'
		var path = unescape( window.location.pathname );

		var drive = "|";
		
		// gets the drive letter and directory path
		if ( ( x = path.lastIndexOf( "|" ) ) > 0 )
		{
			drive = path.substring( path.indexOf( '/' ) + 1, path.indexOf( '|' ) );
			path = path.substring( path.indexOf( '|' ) + 1, path.lastIndexOf( '/' ) + 1 );
		}

		var pathArray = path.split( "/" );
		path = pathArray.join( "\\" );

		//debug( "drive: " + drive + " path: " + path );

		// construct newpath		

		newpath = drive + ":" + path + "\\";
		debug( "path: " + newpath );
	}
	return newpath;
}

function setFocus( theObject )
{
	theObject.focus();
	theObject.select();
}

function message( txt )
{
	window.status = txt;
	setTimeout( "remove_message()", 10000 );
}

function remove_message()
{
	window.status = "";
}

function getSelectedISPName()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	var		ispDisplayName = document.setupPlugin.GetISPDisplayName( selectedISP );				
	
	return ispDisplayName;
}

function checkPluginExists( name, generateOutputFlag )
{
	/*
	var myPlugin = navigator.plugins["name"];
	if (myPlugin)	{
		// do something here
		}
	else	{
		document.writeln("<CENTER><STRONG>Warning! The '" +name+ "' plug-in is not installed!</STRONG></CENTER>\n");
		}
	*/
	
	if ( navigator.javaEnabled() )
	{
		var myMimetype = navigator.mimeTypes[ name ]
		if ( myMimetype )
		{
		 	return true;
		}
		else
		{
			if ( generateOutputFlag == true )
			{
				document.writeln( "<CENTER><STRONG>The 'Account Setup Plugin' is not installed!<P>\n" );
				document.writeln( "Please install the plugin, then run 'Account Setup' again.</STRONG></CENTER>\n" );
			}
		 return false;
		}
	}
	else
	{
		if ( generateOutputFlag==true )
		{
			document.writeln( "<CENTER><STRONG>Java support is disabled!<P>\n" );
			document.writeln( "Choose Options | Network Preferences and enable Java, then try again.</STRONG></CENTER>\n" );
		}
	 	return false;
	}
}



function forceReboot(pageName)
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );
	debug( "go: Reboot needed before move to page " + pageName );
	alert( "A reboot is needed. Account Setup will continue after the reboot." );
	navigator.preference( "mail.check_new_mail", false );
	document.vars.startupFile.value = pageName;
	saveGlobalData();

	// if rebooting and using magic profile, rename it so that it won't be automatically deleted at next launch
	var profileName = parent.parent.globals.document.setupPlugin.GetCurrentProfileName();
	if ( profileName != null && profileName != "" )
	{
		profileName = profileName.toUpperCase();
		if ( profileName == '911' || profileName == 'USER1' )
			parent.parent.globals.document.setupPlugin.SetCurrentProfileName( "912" );
	}

	parent.parent.globals.document.setupPlugin.Reboot( getFolder( self ) + "start.htm" );
}



function findVariable( theVar )
{
	var theValue = "";
	var regData = parent.parent.globals.document.vars.regData.value;
	var x = regData.indexOf( theVar + "=" );
	if ( x >= 0 )
	{
		x = x + theVar.length + 1;
		var y = regData.indexOf( "\r", x );
		if ( y > x )
		{
			theValue = regData.substring( x, y );
			debug( "findVariable: " + theVar + "=" + theValue );
		}
	}
	return theValue;
}



//	contentFile = "main.htm";
function getContentPage()
{
//	var file = contentFile;
//	contentFile = null;
	var file = parent.parent.globals.document.vars.startupFile.value;
	parent.parent.globals.document.vars.startupFile.value = "";
	return file;
}

function setContentPage( file )
{
//	contentFile = file;
	parent.parent.globals.document.vars.startupFile.value = file;
}


function loadUserInput()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (document.vars.inited.value != "yes")	{
		document.vars.inited.value = "yes";

		var cookieWarning = navigator.preference("network.cookie.warnAboutCookies");
		if (cookieWarning == true)	{
			document.vars.cookieWarning.value = "yes";
			}
		else	{
			document.vars.cookieWarning.value = "no";
			}
		navigator.preference("network.cookie.warnAboutCookies", false);

		document.vars.offlineMode.value = navigator.preference("offline.startup_mode");
		navigator.preference("offline.startup_mode", 0);		// online
		navigator.preference("network.online", true);

		var acctSetupFile = getAcctSetupFilename(self);
		if (acctSetupFile != null && acctSetupFile != "")	{
			var intlMode = parent.parent.globals.GetNameValuePair(acctSetupFile,"Mode Selection","IntlMode");
			if (intlMode != null && intlMode != "")	{
				intlMode = intlMode.toLowerCase();
				}
			}
	
		var userInputFile = document.setupPlugin.GetCurrentProfileDirectory();
		if (userInputFile != null && userInputFile != "")	{
			userInputFile = userInputFile + "ACCTSET.DAT";
			var theSection="Account Setup User Input";
		
			var regServer = GetNameValuePair(userInputFile,theSection,"regServer");
			if (regServer != null && regServer != "")	{
				document.vars.regServer.value = regServer;
				SetNameValuePair(userInputFile,theSection,"regServer", "");
				}
			document.vars.first.value = GetNameValuePair(userInputFile,theSection,"first");
			document.vars.last.value = GetNameValuePair(userInputFile,theSection,"last");
			document.vars.company.value = GetNameValuePair(userInputFile,theSection,"company");
			document.vars.address1.value = GetNameValuePair(userInputFile,theSection,"address1");
			document.vars.address2.value = GetNameValuePair(userInputFile,theSection,"address2");
			if (intlMode=="yes")	{
				document.vars.address3.value = GetNameValuePair(userInputFile,theSection,"address3");
				document.vars.city.value="";
				document.vars.state.value="";
				document.vars.zip.value="";
				document.vars.areaCode.value="";
				}
			else	{
				document.vars.address3.value="";
				document.vars.city.value = GetNameValuePair(userInputFile,theSection,"city");
				document.vars.state.value = GetNameValuePair(userInputFile,theSection,"state");
				document.vars.zip.value = GetNameValuePair(userInputFile,theSection,"zip");
				document.vars.areaCode.value = GetNameValuePair(userInputFile,theSection,"areaCode");
				}
			document.vars.phoneNumber.value = GetNameValuePair(userInputFile,theSection,"phoneNumber");
			document.vars.country.value = GetNameValuePair(userInputFile,theSection,"country");
			document.vars.countryCode.value = GetNameValuePair(userInputFile,theSection,"countryCode");
				
			document.vars.cardname.value = GetNameValuePair(userInputFile,theSection,"cardname");
			document.vars.cardtype.value = GetNameValuePair(userInputFile,theSection,"cardtype");
			document.vars.cardcode.value = GetNameValuePair(userInputFile,theSection,"cardcode");
			document.vars.cardnumber.value = GetNameValuePair(userInputFile,theSection,"cardnumber");
			SetNameValuePair(userInputFile,theSection,"cardnumber", "");
	
			document.vars.month.value = GetNameValuePair(userInputFile,theSection,"month");
			document.vars.year.value = GetNameValuePair(userInputFile,theSection,"year");
	
			document.vars.modem.value = GetNameValuePair(userInputFile,theSection,"modem");
			document.vars.manufacturer.value = GetNameValuePair(userInputFile,theSection,"manufacturer");
			document.vars.model.value = GetNameValuePair(userInputFile,theSection,"model");
	
	
			document.vars.externalEditor.value = GetNameValuePair(userInputFile,theSection,"externalEditor");
	
	
			if (intlMode=="yes")	{
				document.vars.modemAreaCode.value="";
				}
			else	{
				document.vars.modemAreaCode.value = GetNameValuePair(userInputFile,theSection,"modemAreaCode");
				}
			document.vars.modemPhoneNumber.value = GetNameValuePair(userInputFile,theSection,"modemPhoneNumber");
			
			document.vars.altAreaCode1.value = GetNameValuePair( userInputFile, theSection, "altAreaCode1" );
			document.vars.altAreaCode2.value = GetNameValuePair( userInputFile, theSection, "altAreaCode2" );
			document.vars.altAreaCode3.value = GetNameValuePair( userInputFile, theSection, "altAreaCode3" );
			
			document.vars.cwData.value = GetNameValuePair(userInputFile,theSection,"cwData");
			if (document.vars.cwData.value != null && document.vars.cwData.value != "")
			{
//				document.vars.prefix.cwOFF=1;
			}
			else
			{
//				document.vars.prefix.cwOFF=0;
			}
			document.vars.prefixData.value = GetNameValuePair(userInputFile,theSection,"prefixData");
			if (document.vars.prefixData.value != null && document.vars.prefixData.value != "")
			{
//				document.vars.prefix.checked=1;
			}
			else
			{
//				document.vars.prefix.checked=0;
			}
			document.vars.dialMethod.value = GetNameValuePair(userInputFile,theSection,"dialMethod");
	
			document.vars.providername.value = GetNameValuePair(userInputFile,theSection,"providername");
			if (intlMode=="yes")
			{
				document.vars.accountAreaCode.value="";
			}
			else
			{
				document.vars.accountAreaCode.value = GetNameValuePair(userInputFile,theSection,"accountAreaCode");
			}
			document.vars.accountPhoneNumber.value = GetNameValuePair(userInputFile,theSection,"accountPhoneNumber");
	
			document.vars.accountName.value = GetNameValuePair(userInputFile,theSection,"accountName");
			document.vars.emailName.value = GetNameValuePair(userInputFile,theSection,"emailName");
			document.vars.publishURL.value = GetNameValuePair(userInputFile,theSection,"publishURL");
			document.vars.viewURL.value = GetNameValuePair(userInputFile,theSection,"viewURL");

			document.vars.accountPassword.value = GetNameValuePair(userInputFile,theSection,"accountPassword");		// existing path
			document.vars.accountPasswordCheck.value = GetNameValuePair(userInputFile,theSection,"accountPasswordCheck");
			document.vars.emailPassword.value = GetNameValuePair(userInputFile,theSection,"emailPassword");
			document.vars.emailPasswordCheck.value = GetNameValuePair(userInputFile,theSection,"emailPasswordCheck");
			document.vars.publishPassword.value = GetNameValuePair(userInputFile,theSection,"publishPassword");
			document.vars.publishPasswordCheck.value = GetNameValuePair(userInputFile,theSection,"publishPasswordCheck");
			document.vars.SMTP.value = GetNameValuePair(userInputFile,theSection,"SMTP");
			document.vars.mailServer.value = GetNameValuePair(userInputFile,theSection,"mailServer");
			document.vars.mailProtocol.value = GetNameValuePair(userInputFile,theSection,"mailProtocol");
			document.vars.NNTP.value = GetNameValuePair(userInputFile,theSection,"NNTP");
			document.vars.domainName.value = GetNameValuePair(userInputFile,theSection,"domainName");
			document.vars.primaryDNS.value = GetNameValuePair(userInputFile,theSection,"primaryDNS");
			document.vars.secondaryDNS.value = GetNameValuePair(userInputFile,theSection,"secondaryDNS");
			document.vars.ipAddress.value = GetNameValuePair(userInputFile,theSection,"ipAddress");
			document.vars.scriptEnabled.value = GetNameValuePair(userInputFile,theSection,"scriptEnabled");
			document.vars.scriptFile.value = GetNameValuePair(userInputFile,theSection,"scriptFile");
			document.vars.lckFilename.value = GetNameValuePair(userInputFile,theSection,"lckFilename");

			SetNameValuePair(userInputFile,theSection,"accountPassword", "");										// existing path
			SetNameValuePair(userInputFile,theSection,"accountPasswordCheck", "");
			SetNameValuePair(userInputFile,theSection,"emailPassword", "");
			SetNameValuePair(userInputFile,theSection,"emailPasswordCheck", "");
			SetNameValuePair(userInputFile,theSection,"publishPassword", "");
			SetNameValuePair(userInputFile,theSection,"publishPasswordCheck", "");
			}
		}
}



/*
	loadGlobalData:		checks for the plugin; reads in any saved user input from a previous session
*/

function loadGlobalData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if ( document.setupPlugin == null )
		return;

	if ( document.vars.inited.value != "yes" )
	{

		// load globals here
	
		var acctSetupFile = getAcctSetupFilename( self );
		if ( acctSetupFile != null && acctSetupFile != "" )
		{
			var intlMode = parent.parent.globals.GetNameValuePair( acctSetupFile, "Mode Selection", "IntlMode" );
			if ( intlMode != null && intlMode != "" )
			{
				intlMode = intlMode.toLowerCase();
				document.vars.intlMode.value = intlMode;
			}
			
			var editMode = parent.parent.globals.GetNameValuePair( acctSetupFile, "Mode Selection", "EditMode" );
			if ( editMode != null && editMode != "" )
			{
				editMode = editMode.toLowerCase();
				document.vars.editMode.value = editMode;
				SetNameValuePair( acctSetupFile, "Mode Selection", "EditMode", "no" );
			}

			// if not in editMode, ensure that OS support is available (networking,dialer,etc)
			if ( editMode != "yes" )
			{
				var installedFlag = document.setupPlugin.CheckEnvironment();
				if ( installedFlag != true )
				{
					document.setupPlugin.QuitNavigator();
					return;
				}
			}

			// set appropriate path info (if not prompting user)
	
			document.vars.path.value = "";
			var newPathFlag = parent.parent.globals.GetNameValuePair( acctSetupFile, "Mode Selection", "ForceNew" );
			newPathFlag = newPathFlag.toLowerCase();
			var existingPathFlag = parent.parent.globals.GetNameValuePair( acctSetupFile, "Mode Selection", "ForceExisting" );
			existingPathFlag = existingPathFlag.toLowerCase();
	
			if ( newPathFlag == "yes" && existingPathFlag != "yes" )
				document.vars.path.value = "New Path";
			else if ( existingPathFlag == "yes" && newPathFlag != "yes" )
				document.vars.path.value = "Existing Path";

			document.vars.oneStepMode.value = "";
			var oneStepModeFlag = parent.parent.globals.GetNameValuePair( acctSetupFile, "Mode Selection", "OneStepMode" );
			oneStepModeFlag = oneStepModeFlag.toLowerCase();
			if ( oneStepModeFlag == "yes" )
				document.vars.oneStepMode = "yes";
						
			if ( document.vars.debugMode.value.toLowerCase() != "yes" && ( document.vars.editMode.value.toLowerCase() != "yes" ) )
				if (checkPluginExists( "application/x-netscape-autoconfigure-dialer", false ) )
					document.setupPlugin.SetKiosk( true );
		}
		
		
		// load in user input (if any)
		var userInputFile = document.setupPlugin.GetCurrentProfileDirectory();
		if ( userInputFile != null && userInputFile != "" )
		{

			userInputFile = userInputFile + "ACCTSET.DAT";
			var theSection = "Account Setup User Input";
	
			document.vars.externalEditor.value = GetNameValuePair( userInputFile, theSection, "externalEditor" );

			var startupFile = GetNameValuePair( userInputFile, theSection, "startupFile" );
			if ( startupFile != null && startupFile != "" )
			{
				document.vars.startupFile.value = startupFile;
				document.vars.path.value = GetNameValuePair( userInputFile, theSection, "path" );
				document.vars.pageHistory.value = GetNameValuePair( userInputFile, theSection, "pageHistory" );

				SetNameValuePair( userInputFile, theSection, "startupFile", "" );
				SetNameValuePair( userInputFile, theSection, "path", "" );
				SetNameValuePair( userInputFile, theSection, "pageHistory", "" );

				// if coming out of a reboot and using magic profile, rename it so that it won't be automatically deleted at next launch
			
				var profileName = document.setupPlugin.GetCurrentProfileName();
				if ( profileName != null && profileName != "" )
				{
					profileName = profileName.toUpperCase();
					if ( profileName == '912' )
						document.setupPlugin.SetCurrentProfileName( "USER1" );
				}
				loadUserInput();
			}
			else
			{
				document.vars.startupFile.value = "main.htm";
				// defer loading user input until after main screen has loaded (faster speedup)
			}
			parent.screen.location.replace( "screen.htm" );
		}

		// QA support

		var qaMode = document.vars.qaMode.value;
		if ( qaMode != null && qaMode != "" )
		{
			qaMode = qaMode.toLowerCase();
			if ( qaMode == "yes" )
			{
				if ( confirm( "Would you like to use a Milan data file to configure Communicator?" ) == true )
				{
					if ( document.setupPlugin.Milan( null, null, true, false ) == true )
						document.vars.regMode.value = "yes";
				}
			}
		}
	}
	
	if ( document.vars.regMode.value == "yes" )
		setRegisterMode( 1 );
}

function saveExternalEditor()
{
	// Since we don't do a saveGlobalData in editMode, we need an alternate way to save the externalEditor
	// as a preference in ACCTSET.DAT.  This is it.
	
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );
	var userInputFile = document.setupPlugin.GetCurrentProfileDirectory();
	if ( userInputFile != null && userInputFile != "" )
	{
		userInputFile = userInputFile + "ACCTSET.DAT";
		var theSection = "Account Setup User Input";
		SetNameValuePair( userInputFile, theSection, "externalEditor", document.vars.externalEditor.value );
	}
}

function saveGlobalData()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	if ( document.vars.editMode.value.toLowerCase() == "yes" )
		return;

	if ( document.setupPlugin == null )
		return;
/*
	if (document.vars.debugMode.value.toLowerCase() != "yes" && (document.vars.editMode.value.toLowerCase() != "yes"))	{
		if (checkPluginExists("application/x-netscape-autoconfigure-dialer",false))	{
			document.setupPlugin.SetKiosk(false);
			}
		}
	top.statusbar.visible=true;
	top.scrollbars.visible=true;
	top.toolbar.visible=true;
	top.menubar.visible=true;
	top.locationbar.visible=true;
	top.personalbar.visible=true;				// was directory
*/
		
	if (document.vars.cookieWarning.value == "yes")
	{
		navigator.preference("network.cookie.warnAboutCookies", true);
	}
	else
	{
		navigator.preference("network.cookie.warnAboutCookies", false);
	}

	if (document.vars.offlineMode.value != "undefined")
	{
		navigator.preference("offline.startup_mode", document.vars.offlineMode.value);
	}


	// save user input (if any)
	var userInputFile = document.setupPlugin.GetCurrentProfileDirectory();
	if (userInputFile != null && userInputFile != "")
	{
		userInputFile = userInputFile + "ACCTSET.DAT";
		var theSection="Account Setup User Input";
	
		SetNameValuePair(userInputFile,theSection,"startupFile", document.vars.startupFile.value);
		SetNameValuePair(userInputFile,theSection,"regServer", document.vars.regServer.value);

		SetNameValuePair(userInputFile,theSection,"first", document.vars.first.value);
		SetNameValuePair(userInputFile,theSection,"last", document.vars.last.value);
		SetNameValuePair(userInputFile,theSection,"company", document.vars.company.value);
		SetNameValuePair(userInputFile,theSection,"address1", document.vars.address1.value);
		SetNameValuePair(userInputFile,theSection,"address2", document.vars.address2.value);
		SetNameValuePair(userInputFile,theSection,"address3", document.vars.address3.value);
		SetNameValuePair(userInputFile,theSection,"city", document.vars.city.value);
		SetNameValuePair(userInputFile,theSection,"state", document.vars.state.value);
		SetNameValuePair(userInputFile,theSection,"zip", document.vars.zip.value);
		SetNameValuePair(userInputFile,theSection,"areaCode", document.vars.areaCode.value);
		SetNameValuePair(userInputFile,theSection,"phoneNumber", document.vars.phoneNumber.value);
		SetNameValuePair(userInputFile,theSection,"country", document.vars.country.value);
		SetNameValuePair(userInputFile,theSection,"countryCode", document.vars.countryCode.value);
		
		SetNameValuePair(userInputFile,theSection,"cardname", document.vars.cardname.value);
		SetNameValuePair(userInputFile,theSection,"cardtype", document.vars.cardtype.value);
		SetNameValuePair(userInputFile,theSection,"cardcode", document.vars.cardcode.value);
		if (document.setupPlugin.NeedReboot() == true)	{
			SetNameValuePair(userInputFile,theSection,"cardnumber", document.vars.cardnumber.value);
			SetNameValuePair(userInputFile,theSection,"path", document.vars.path.value);
			SetNameValuePair(userInputFile,theSection,"pageHistory", document.vars.pageHistory.value);

			SetNameValuePair(userInputFile,theSection,"accountPassword", document.vars.accountPassword.value);		// existing path
			SetNameValuePair(userInputFile,theSection,"accountPasswordCheck", document.vars.accountPasswordCheck.value);
			SetNameValuePair(userInputFile,theSection,"emailPassword", document.vars.emailPassword.value);
			SetNameValuePair(userInputFile,theSection,"emailPasswordCheck", document.vars.emailPasswordCheck.value);
			SetNameValuePair(userInputFile,theSection,"publishPassword", document.vars.publishPassword.value);
			SetNameValuePair(userInputFile,theSection,"publishPasswordCheck", document.vars.publishPasswordCheck.value);
			SetNameValuePair(userInputFile,theSection,"SMTP", document.vars.SMTP.value);
			SetNameValuePair(userInputFile,theSection,"mailServer", document.vars.mailServer.value);
			SetNameValuePair(userInputFile,theSection,"mailProtocol", document.vars.mailProtocol.value);
			SetNameValuePair(userInputFile,theSection,"NNTP", document.vars.NNTP.value);
			SetNameValuePair(userInputFile,theSection,"domainName", document.vars.domainName.value);
			SetNameValuePair(userInputFile,theSection,"primaryDNS", document.vars.primaryDNS.value);
			SetNameValuePair(userInputFile,theSection,"secondaryDNS", document.vars.secondaryDNS.value);
			SetNameValuePair(userInputFile,theSection,"ipAddress", document.vars.ipAddress.value);
			SetNameValuePair(userInputFile,theSection,"scriptEnabled", document.vars.scriptEnabled.value);
			SetNameValuePair(userInputFile,theSection,"scriptFile", document.vars.scriptFile.value);
			SetNameValuePair(userInputFile,theSection,"lckFilename", document.vars.lckFilename.value);
		}
		else
		{
			SetNameValuePair(userInputFile,theSection,"cardnumber", "");
			SetNameValuePair(userInputFile,theSection,"path", "");
			SetNameValuePair(userInputFile,theSection,"pageHistory", "");

			SetNameValuePair(userInputFile,theSection,"accountPassword", "");										// existing path
			SetNameValuePair(userInputFile,theSection,"accountPasswordCheck", "");
			SetNameValuePair(userInputFile,theSection,"emailPassword", "");
			SetNameValuePair(userInputFile,theSection,"emailPasswordCheck", "");
			SetNameValuePair(userInputFile,theSection,"publishPassword", "");
			SetNameValuePair(userInputFile,theSection,"publishPasswordCheck", "");
		}
		SetNameValuePair(userInputFile,theSection,"month", document.vars.month.value);
		SetNameValuePair(userInputFile,theSection,"year", document.vars.year.value);

		SetNameValuePair(userInputFile,theSection,"modem", document.vars.modem.value);
		SetNameValuePair(userInputFile,theSection,"manufacturer", document.vars.manufacturer.value);
		SetNameValuePair(userInputFile,theSection,"model", document.vars.model.value);

		SetNameValuePair(userInputFile,theSection,"modemAreaCode", document.vars.modemAreaCode.value);
		SetNameValuePair(userInputFile,theSection,"modemPhoneNumber", document.vars.modemPhoneNumber.value);
		SetNameValuePair(userInputFile,theSection,"altAreaCode1", document.vars.altAreaCode1.value);
		SetNameValuePair(userInputFile,theSection,"altAreaCode2", document.vars.altAreaCode2.value);
		SetNameValuePair(userInputFile,theSection,"altAreaCode3", document.vars.altAreaCode3.value);

		SetNameValuePair(userInputFile,theSection,"cwData", document.vars.cwData.value);
		SetNameValuePair(userInputFile,theSection,"prefixData", document.vars.prefixData.value);
		SetNameValuePair(userInputFile,theSection,"dialMethod", document.vars.dialMethod.value);

		SetNameValuePair(userInputFile,theSection,"providername", document.vars.providername.value);
		SetNameValuePair(userInputFile,theSection,"providerFilename", document.vars.providerFilename.value);		// existing path
		SetNameValuePair(userInputFile,theSection,"accountAreaCode", document.vars.accountAreaCode.value);
		SetNameValuePair(userInputFile,theSection,"accountPhoneNumber", document.vars.accountPhoneNumber.value);

		SetNameValuePair(userInputFile,theSection,"accountName", document.vars.accountName.value);
		SetNameValuePair(userInputFile,theSection,"emailName", document.vars.emailName.value);
		SetNameValuePair(userInputFile,theSection,"publishURL", document.vars.publishURL.value);
		SetNameValuePair(userInputFile,theSection,"viewURL", document.vars.viewURL.value);
		SetNameValuePair(userInputFile,theSection,"externalEditor", document.vars.externalEditor.value);
		}
}

/*
function monitorDialupConnection( numSecondsElapsed, monitorFunction,
	monitorEndpointSuccessFunction, monitorEndpointFailureFunction )
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	var		connectStatusFlag = document.setupPlugin.IsDialerConnected();
	
	// * give us 10 seconds to before we start to check the connection status
	if ( ( numSecondsElapsed < 10 ) || ( connectStatusFlag == true ) )
	{
		numSecondsElapsed = numSecondsElapsed + 1;
		
		if ( eval ( monitorFunction ) == null )
			setTimeout( "setRegisterMode(" + numSecondsElapsed + ")", 1000 );
		else
			eval( monitorEndpointSuccessFunction );
	}
	else
	{
		// hang up (even if already disconnected, this will delete
		// the dialer's reference to the Registration Server)
		document.setupPlugin.DialerHangup();
		eval( monitorEndpointFailureFunction );
	}
}
*/

function set1StepMode( numSecondsElapsed )
{
	
	debug( "set1StepMode: " + numSecondsElapsed );
	
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	var			connectStatusFlag = document.setupPlugin.IsDialerConnected();

	if ( ( numSecondsElapsed < 10 ) || ( connectStatusFlag == true ) )
	{
		//debug( "still connected" );
		numSecondsElapsed = numSecondsElapsed + 1;

		if ( oneStepSemaphore == false )
		{
			setTimeout( "set1StepMode(" + numSecondsElapsed + ")", 1000 );
		}
		else
		{
			oneStepSemphore = false;
		}
	}
	else
	{
		document.setupPlugin.DialerHangup();

		// go to error screen
		setContentPage( "error.htm" );
		parent.screen.location.replace( "screen.htm" );
	}
}
			
		
		
function setRegisterMode( numSecondsElapsed )
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	var			connectStatusFlag = document.setupPlugin.IsDialerConnected();
	document.vars.regMode.value = "yes";
	var			regData = document.setupPlugin.GetRegInfo( false );

	if ( ( numSecondsElapsed < 10 ) || ( connectStatusFlag == true ) )
	{
		numSecondsElapsed = numSecondsElapsed + 1;

		if ( regData == null )
			setTimeout( "setRegisterMode(" + numSecondsElapsed + ")", 1000 );	// check every second
		else
		{
			// handle multi-line data differently

			var bookmarkTag = "BOOKMARK_FILE=";
			var bookmarkTagLen = bookmarkTag.length;

			document.vars.regMode.value = "no";
			document.vars.regData.value = "";
			debug( "\nRegistration Complete: " + regData.length + " item(s)" );
			
			for ( var x=0; x < regData.length; x++ )
			{
				var data = "" + regData[x];
				var dataLen = data.length;

				if ( data.indexOf( bookmarkTag ) ==0 )
				{
					data = data.substring( bookmarkTagLen, dataLen );
					document.vars.regBookmark.value = data;
				}
				else
					document.vars.regData.value = document.vars.regData.value + regData[ x ] + "\r";

				debug( "        " + x + ": " + regData[ x ] );
			}
			
			document.setupPlugin.GetRegInfo( true );

			// hang up (this will delete the dialer's
			// reference to the Registration Server)
			document.setupPlugin.DialerHangup();

			// check status and go to appropriate screen

			var status = findVariable( "STATUS" );
			if ( status == "OK" )
			{
				configureNewAccount();

				var rebootFlag = document.setupPlugin.NeedReboot();
				if ( rebootFlag == true )
					setContentPage( "okreboot.htm" );
				else
					setContentPage( "ok.htm" );

			}
			else if ( status == "EXIT" )
			{
				saveGlobalData();
				document.setupPlugin.QuitNavigator();
			}
			else
				setContentPage( "error.htm" );
				
			parent.frames[ 0 ].location.replace( "screen.htm" );

			navigator.preference( "security.warn_submit_insecure", true );
			navigator.preference( "security.warn_entering_secure", true );
			navigator.preference( "security.warn_leaving_secure", true );
		}
	}

	// * we've lost the connection
	else
	{
		document.vars.regMode.value = "no";

		// hang up (even if already disconnected, this will delete
		// the dialer's reference to the Registration Server)
		document.setupPlugin.DialerHangup();

		// go to error screen
		document.setupPlugin.GetRegInfo( true );
		setContentPage( "error.htm" );
		parent.screen.location.replace( "screen.htm" );

		navigator.preference( "security.warn_submit_insecure", true );
		navigator.preference( "security.warn_entering_secure", true );
		navigator.preference( "security.warn_leaving_secure", true );
	}
}



function verifyIPaddress( address )
{
	var dotCount = 0, dotIndex = 0, net, validFlag = false;

	while ( dotIndex >= 0 )
	{
		net = "";
		dotIndex = address.indexOf( "." );
		if ( dotIndex >=0 )
		{
			net = address.substring( 0, dotIndex );
			address = address.substring( dotIndex + 1 );
			++dotCount;
		}
		else
		{
			net = address;
			if ( net=="" )
				break;
		}

		netValue = parseInt( net );
		if ( isNaN( netValue ) )
			break;
		if ( netValue < 0 || netValue > 255 )
			break;

		if ( dotCount == 3 && dotIndex < 0 )
			validFlag = true;
	}	
	return validFlag;
}

function verifyAreaCode( areaCode )
{
	var	validFlag = false;

	if ( areaCode.length >= 3 )
	{
		validFlag = true;
		for ( var x = 0; x < areaCode.length; x++ )
		{
			if ( "0123456789".indexOf( areaCode.charAt( x ) ) < 0 )
			{
				validFlag = false;
				break;
			}
		}
	}
	return validFlag;
}

function verifyZipCode( zipCode )
{
	var	validFlag = false;

	if ( zipCode.length >= 5 )
	{
		validFlag = true;
		for ( var x = 0; x < zipCode.length; x++ )
		{
			if ( "0123456789-".indexOf( zipCode.charAt( x ) ) < 0 )
			{
				validFlag = false;
				break;
			}
		}
	}
	return validFlag;
}

function verifyPhoneNumber( phoneNum )
{
	var	validFlag = false;

	if ( phoneNum.length >= 7 )
	{
		validFlag = true;
		for ( var x=0; x < phoneNum.length; x++ )
		{
			if ( "0123456789().,-+ ".indexOf( phoneNum.charAt( x ) ) < 0 )
			{
				validFlag = false;
				break;
			}
		}
	}
	return validFlag;
}



// end hiding contents from old browsers  -->
