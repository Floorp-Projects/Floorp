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

function go( msg )
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	// * skip if we're in edit mode
	if ( parent.parent.globals.document.vars.editMode.value != "yes" )
	{
		if ( document.forms && document.forms[ 0 ] && document.forms[ 0 ].popList )
			parent.parent.globals.document.setupPlugin.CreateConfigIAS(
				parent.parent.globals.selectedISP, document.forms[ 0 ].popList.selectedIndex );
		else
			parent.parent.globals.document.setupPlugin.CreateConfigIAS(
				parent.parent.globals.selectedISP, -1 );
				
		return true;
	}
	else
	{
		return false;
	}
}

function checkData()
{
	return true;
}

function loadData()
{
	if ( parent.controls.generateControls )
		parent.controls.generateControls();
}

function saveData()
{
}

function generatePopNumberList()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	var list = parent.parent.globals.document.setupPlugin.GetISPPopList(
					parent.parent.globals.selectedISP );
	
	parent.parent.globals.debug( "generating pop list" );
	if ( list && list.length > 0 )
	{
		parent.parent.globals.debug( "emitting table" );
		document.writeln( "<TABLE CELLPADDING=2 CELLSPACING=0 ID='minspace'><TR><TD ALIGN=LEFT VALIGN=TOP HEIGHT=25><spacer type=vertical size=2><B>Pick a phone number from the following list to connect to:</B></TD><TD ALIGN=LEFT VALIGN=TOP><FORM><SELECT NAME='popList'>");
		for ( var x = 0; x < list.length; x++ )
		{
			var name = list[ x ];
			var selected = ( x == 0 ) ? " SELECTED" : "";
			document.writeln( "<OPTION VALUE='" + x + "'" + selected + ">" + name );
		}
		document.writeln( "</SELECT></FORM></TD></TR></TABLE>" );
	}
}

// end hiding contents from old browsers  -->
