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

	// ¥ skip if we're in edit mode
	if ( parent.parent.globals.document.vars.editMode.value != "yes" )
	{
		// ¥ if "RegServer" specified in ACCTSET.INI, use it
		var theFile = parent.parent.globals.getAcctSetupFilename( self );
		var intlFlag = parent.parent.globals.GetNameValuePair( theFile, "Mode Selection", "IntlMode" );
		intlFlag = intlFlag.toLowerCase();
		var theRegFile = parent.parent.globals.GetNameValuePair( theFile, "New Acct Mode", "RegServer" );
		
		if ( theRegFile != null && theRegFile != "" )
		{
			parent.parent.globals.document.vars.regServer.value = theRegFile;
		}
		else
		{
			// ¥ otherwise, if multiple .IAS files exist, get list selection and determine appropriate .IAS file
			var pathName = parent.parent.globals.getConfigFolder( self );
			var theList = parent.parent.globals.document.setupPlugin.GetFolderContents( pathName,".IAS" );

			if ( theList != null )
			{
				if ( theList.length > 1 )
				{
					if ( document.forms[ 0 ].regServerList.selectedIndex < 0 )
					{
						alert( "Please select an Internet account server." );
						return false;
					}
					
					for ( var x = 0; x < theList.length; x++ )	
					{
						var file = parent.parent.globals.getConfigFolder( self ) + theList[ x ];
						var name = parent.parent.globals.document.setupPlugin.GetNameValuePair( file, "Dial-In Configuration", "SiteName" );
	
						if ( name == document.forms[ 0 ].regServerList.options[ document.forms[ 0 ].regServerList.selectedIndex ].text )
						{
							parent.parent.globals.document.vars.regServer.value = theList[ x ];
							break;
						}
					}
					
					if ( parent.parent.globals.document.vars.regServer.value == "" )
					{
						alert( "Internal problem locating appropriate registration server file." );
						return false;
					}
				}
				else if ( theList.length == 1 )
				{
					parent.parent.globals.document.vars.regServer.value = theList[ 0 ];
				}
				else
				{
					alert( "Internal problem locating a registration server file." );
					return false;
				}
			}
			else
			{
				alert( "Internal problem locating appropriate registration server file." );
				return false;
			}
		}
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



function generateRegServerList()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	// if RegServer is not specified in ACCTSET.INI and multiple .IAS files exist, build list

	var theFile = parent.parent.globals.getAcctSetupFilename( self );
	var theRegFile = parent.parent.globals.GetNameValuePair( theFile, "New Acct Mode", "RegServer" );
	
	if ( theRegFile == null || theRegFile == "" )
	{
		var pathName = parent.parent.globals.getConfigFolder( self );
		var theList = parent.parent.globals.document.setupPlugin.GetFolderContents( pathName, ".IAS" );
		if ( theList != null )
		{
			if ( theList.length > 1 )	
			{
				document.writeln( "<TABLE CELLPADDING=2 CELLSPACING=0 ID='minspace'><TR><TD ALIGN=LEFT VALIGN=TOP HEIGHT=25><spacer type=vertical size=2><B>Select an Internet account server:</B></TD><TD ALIGN=LEFT VALIGN=TOP><FORM><SELECT NAME='regServerList'>");
				for ( var x = 0; x < theList.length; x++ )
				{
					var file = parent.parent.globals.getConfigFolder( self ) + theList[ x ];
					var name = parent.parent.globals.document.setupPlugin.GetNameValuePair( file, "Dial-In Configuration", "SiteName" );
					var selected = ( x== 0 ) ? " SELECTED" : "";
					document.writeln( "<OPTION VALUE='" + name + "'" + selected + ">" + name );
				}
				document.writeln( "</SELECT></FORM></TD></TR></TABLE>" );
			}
		}
	}
}



// end hiding contents from old browsers  -->
