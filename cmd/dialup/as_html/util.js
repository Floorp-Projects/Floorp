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
	var path = unescape( window.location.pathname );

	if ( platform == "Macintosh" )
	{				// Macintosh support
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
		var drive = "|";

		// gets the drive letter and directory path
		if ( ( x = path.lastIndexOf( "|" ) ) > 0 )
		{
			slash = path.indexOf( '/' );
			bar = path.indexOf( '|' );
		
			if ( slash < bar )
				drive = path.substring( slash + 1, bar );
			else
				drive = path.substring( bar - 1, bar );
			
			path = path.substring( bar + 1, path.lastIndexOf( '/' ) + 1 );
		}

		var pathArray = path.split( "/" );
		path = pathArray.join( "\\" );
		if ( path.charAt( path.length - 1 ) != '\\' )
			path = path + "\\";

		//debug( "drive: " + drive + " path: " + path );

		// construct newpath		
		newpath = drive + ":" + path;
		debug( "path: " + newpath );
	}
	return newpath;
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

function getBrowserVersionNumber()
{
	var theAgent = navigator.userAgent;
	var x = theAgent.indexOf( "/" );
	var theVersion = "unknown";
	if ( x >= 0 )
		var theVersion = theAgent.substring( x + 1, theAgent.length );
	return theVersion;
}

function getBrowserMajorVersionNumber()
{
	var	version = getBrowserVersionNumber();
	if ( version != "unknown" )
	{
		var x = theVersion.indexOf( "." );
		if ( x > 0 )
			version = version.substring( 0, x );	
	}
	return version;
}

function setFocus( theObject )
{
	theObject.focus();
	theObject.select();
}

function setMessage( txt )
{
	window.status = txt;
	setTimeout( "clearMessage()", 10000 );
}

function clearMessage()
{
	window.status = "";
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
			return true;
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

function verifyPhoneNumber( phoneNum, minLength )
{
	var	validFlag = false;

	if ( phoneNum.length >= minLength )
	{
		validFlag = true;
		for ( var x = 0; x < phoneNum.length; x++ )
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

function getAreaCode( tapiPhoneNumber )
{
	var x = tapiPhoneNumber.indexOf( "(" );
	var y = tapiPhoneNumber.indexOf( ")" );
	var	temp = "";
	if ( x >= 0 && y >= 0 )
	{
		temp = tapiPhoneNumber.substring( x + 1, y );
		if ( verifyAreaCode( temp ) == false )
			temp = "";
	}
	return temp;
}

// end hiding contents from old browsers  -->

