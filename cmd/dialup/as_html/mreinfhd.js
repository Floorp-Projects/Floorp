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

var globals = parent.parent.parent.globals;
var plugin = globals.document.setupPlugin;
var controls = parent.parent.controls;

function loadData()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	//globals.debug( "generating more info page" );
	
	result = plugin.GenerateMoreInfoPage( globals.selectedISP );

	if ( result == true )
		parent.moreinfo.location.replace( "ispplans.htm" );
	else
	{
		alert( "Internal error generating listing of ISP plans." );
		return;
	}
	
	if ( controls.generateControls )
		controls.generateControls();
}

function generateHeader()
{
//	globals.debug( "ispDisplayName" + ispDisplayName );

	document.writeln( "More Information about " + globals.getSelectedISPName() );
}

// end hiding contents from old browers -->
