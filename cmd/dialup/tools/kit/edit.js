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
	
//globals
var thePlatform;
var pathFromKitToData = "../Dialup/"

netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );
enableExternalCapture(); 												// This requires UniversalBrowserWrite access
parent.captureEvents(Event.MOUSEDOWN | Event.MOUSEUP | Event.DRAGDROP | Event.DBLCLICK);
parent.onmousedown = cancelEvent;
parent.onmouseup = cancelEvent;
parent.ondragdrop = cancelEvent;

function debug( theString )
{
	java.lang.System.out.println( theString );
}

function cancelEvent(e)
{
	var retVal = false;

	if ( ( e.which < 2 ) && ( e.type != "dragdrop" ) && ( e.type != "dblclick" ) )
		retVal = routeEvent( e );
	return retVal;
}

function baseName( fileRef )
{
	//alert(fileRef);
	fileRef = "" + fileRef;
	fileRef = unescape( fileRef );	
	//alert(fileRef);
	debug( "fileRef2: " + fileRef );
	if ( fileRef.substring( fileRef.length - 1, fileRef.length ) == '/' )
	{
		fileRef = fileRef.substring( 0, fileRef.length - 1 );
		var x = fileRef.lastIndexOf( "/" );
		if ( x >= 0 )
			fileRef = fileRef.substring( x + 1, fileRef.length );
	}
	else
		fileRef = "";
	fileRef = unescape( fileRef );
	return fileRef ;
}

function makeLocalPath( fileRef )
{
	var filePath = new String( fileRef );

	if ( filePath.substring( 0, 8 ) == "file:///" )
		filePath = filePath.substring( 8, filePath.length )
	else if ( filePath.substring(0,7) == "file://" )
		filePath = filePath.substring( 7, filePath.length )

	var		path = unescape( filePath );

	if ( thePlatform == "Macintosh" )
	{				// Macintosh support

		//var path=unescape(filePath);
		//var path = new String( filePath );
		if ( ( x = path.lastIndexOf( "/" ) ) > 0 )
			path = path.substring( 0, x + 1 );

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
				drive = path.substring( slash, bar );
			else
				drive = path.substring( bar - 1, bar );
				
			path = path.substring( bar + 1, path.lastIndexOf( '/' ) + 1 );
		}

		var pathArray = path.split( "/" );
		path = pathArray.join( "\\" );

		//debug( "drive: " + drive + " path: " + path );

		// construct newpath		
		newpath = drive + ":" + path;
		debug( "path: " + newpath );
	}
		
	return newpath;
}

function getListing( thePlatform )
{
	var theLoc;
	
	if ( thePlatform == "Macintosh" )
		theLoc = "../../Dialup/";
	else
		theLoc = "../Dialup/";
		
	//parent.listing.location = theLoc;
	//alert("listing set");
	return theLoc;
}

function loadData()
{

	//get the platform, just once, set the global
	thePlatform = new String( navigator.userAgent );
	var x = thePlatform.indexOf( "(") + 1;
	var y = thePlatform.indexOf( ";", x + 1 );
	thePlatform = thePlatform.substring( x, y );

	// Request privilege

	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );
	compromisePrincipals();
		
	//tell the parent frameset to load the correct folder listing into the invisible frame.
	pathFromKitToData = getListing( thePlatform );
	
	if ( document.forms[ 0 ].packages )	
	{
		var x;
		var fileRef, baseFileRef, pathRef, acctsetFileRef, theFileContents;

		for ( x = document.forms[ 0 ].packages.length - 1; x >= 0; x-- )
			document.forms[ 0 ].packages.options[ x ] = null;
			
		x = 0;

		// Note: start y at 1 to skip "Up to higher level directory" link

		for ( var y = 1; y < parent.listing.document.links.length; y++ )
		{
			fileRef = parent.listing.document.links[ y ] ;

			debug( "fileRef: " + fileRef );
			// baseName rips off everything but the last part of the path
			baseFileRef = baseName( fileRef );
			pathRef = makeLocalPath( fileRef );
			
			debug( "pathRef: " + pathRef );
			debug( "baseFileRef: " + baseFileRef );
			
			if ( baseFileRef != null && baseFileRef != "" )	
			{
				//do some more checking to see if this looks like a valid data folder
				//the check is currently to see if the contents of the ACCTSET.INI file are non-null
				acctsetFileRef = ( pathRef + "Config\\ACCTSET.INI" );
				debug( "acctsetFileRef: " + acctsetFileRef );
				theFileContents = "";
				
				if ( acctsetFileRef && acctsetFileRef != null && acctsetFileRef != "" )
					theFileContents = document.setupPlugin.GetFileContents( acctsetFileRef );

				debug( "theFileContents: " + theFileContents );
				if ( theFileContents != null && theFileContents != "" )
				{
					document.forms[ 0 ].packages.options[ x ] = new Option( baseFileRef, pathRef, false, false );
					x = x + 1;
				}
			}
		}
		document.forms[ 0 ].packages.selectedIndex = 0;
		document.forms[ 0 ].packages.focus();
	}
}



function customize()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");
	compromisePrincipals();
	
	if ( document.forms[ 0 ].packages.selectedIndex < 0 )
	{
		alert("Please select a package to customize.");
		return false;
	}
	
	var startFile = document.forms[ 0 ].packages.options[ document.forms[ 0 ].packages.selectedIndex ].text;
	document.forms[ 0 ].packageRef.value = startFile;
	
	//alert("../../" + startFile + "/start.htm");
	
	var acctsetFile = document.forms[0].packages.options[document.forms[0].packages.selectedIndex].value;
	
	//THE FOLLOWING IS RELATIVE PATH DEPENDENT (i.e. if the tools folder moves, it must be changed)
	if ( thePlatform == "Macintosh" )
		acctsetFile = ( acctsetFile + "Config:ACCTSET.INI" );
	else
		acctsetFile = ( acctsetFile + "Config\\ACCTSET.INI" );		
	
	//alert("acctset.ini is: " + acctsetFile);  
	document.setupPlugin.SetNameValuePair( acctsetFile, "Mode Selection", "EditMode", "yes" );

	//debug ("Acct file = " + acctsetFile);
	//debug ("Set the Edit Mode to " + document.setupPlugin.GetNameValuePair( acctsetFile, "Mode Selection", "EditMode") );
	
	//clayer.js now takes care of opening this window when the time is right
	//var configWindow = top.open("config.htm","Configurator","dependent=yes,alwaysraised=yes,width=400,height=65,toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no");
	
	top.location = pathFromKitToData + startFile + "/start.htm";
	
	return true;
}

// end hiding contents from old browsers  -->
