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

var globals = parent.parent.globals;
var controls = parent.controls;
var plugin = globals.document.setupPlugin;

function go( msg )
{
	return true;
}

function checkData()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	globals.debug( "checkData" );
	// * skip if we're in edit mode
	if ( globals.document.vars.editMode.value != "yes" )
	{
		globals.debug( "not editing" );
		
		if ( document.forms != null && document.forms[ 0 ] != null && document.forms[ 0 ].popList != null )
		{
			globals.debug( "top case" );
			selectedIndex = document.forms[ 0 ].popList.selectedIndex;
		}
		else
		{
			selectedIndex = -1;
		}
		
		globals.debug( "selectedISP: " + globals.selectedISP );
		globals.debug( "selectedIndex: " + selectedIndex );		

		// * check for toll calls when not in international mode
		if ( globals.document.vars.intlMode.value != "yes" )
		{
			ispPhoneNumber = new String( plugin.GetISPModemNumber( globals.selectedISP, selectedIndex ) );
			areaCode = globals.getAreaCode( ispPhoneNumber );
			if ( areaCode != "" )
			{
				globals.debug( "areaCode: " + areaCode );s
				if ( areaCode != "800" && areaCode != "888" )
					if ( confirm( "The area code to call this ISP is " + areaCode + ".  This might be a toll call from your area code.  Do you wish to continue?" ) == false )
						return false;
			}
		}

		globals.debug( "calling createConfigIAS" );
		plugin.CreateConfigIAS( globals.selectedISP, selectedIndex );
		
		globals.debug( "opening support window" );
	
		if (	!globals.supportWindow || globals.supportWindow == null || 
				!globals.supportWindow.document || globals.supportWindow.document == null ||
				globals.supportWindow.closed )
		{			
			globals.supportWindow = top.open( "", "supportNumber",
				"alwaysRaised,dependent=yes,innerHeight=13,innerWidth=550,titleBar=no" );
			globals.supportWindow.moveBy( 2, -18 );
			globals.supportWindow.opener.focus();
		}
		
		ispSupportNumber = plugin.GetISPSupportPhoneNumber( globals.selectedISP );
		ispSupportName = globals.getSelectedISPName();
	
		if ( ispSupportNumber != null && ispSupportNumber != "" )
			globals.supportWindow.document.writeln( "<LAYER PAGEX='0' PAGEY='1'><P><CENTER><FONT FACE='PrimaSans BT' SIZE=-1>For problems, please call " +
				ispSupportName + " at <B>" + ispSupportNumber + "</B></FONT></CENTER></P></LAYER>" );

		return true;
	}
	else
		return false;
}

function saveData()
{
}

function insertISPName()
{
	document.write( globals.getSelectedISPName() );
}

function loadData()
{
	//parent.twostepfooter.document.writeln( "<BODY BACKGROUND='images/bg.gif' BGCOLOR='cccccc'>" );
	//parent.twostepfooter.document.writeln( "<P>If you have trouble setting up your account call " );
	//parent.twostepfooter.document.writeln( globals.getSelectedISPName() );
	//parent.twostepfooter.document.writeln( "at (support number).</P>" );
	//parent.twostepfooter.document.close();

	if ( controls.generateControls )
		controls.generateControls();
}

function generatePopNumberList()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	var list = globals.document.setupPlugin.GetISPPopList( globals.selectedISP );
	
	//globals.debug( "generating pop list" );
	if ( list && list.length > 0 )
	{
		//globals.debug( "emitting table" );
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
