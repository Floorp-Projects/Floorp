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



var	intlString = "";
var	localString = "";



var theFile = parent.parent.globals.getAcctSetupFilename(self);
var intlFlag = parent.parent.globals.GetNameValuePair(theFile,"Mode Selection","IntlMode");
intlFlag = intlFlag.toLowerCase();

if ( intlFlag == "yes" )
{
	intlString = "text";
	localString = "hidden";
}
else
{
	intlString = "hidden";
	localString = "text";
}



function writeLocalText( theString )
{
	if ( localString == "text" )
	{
		document.write( theString );
	}
}



function go( msg )
{
	if ( ( parent.parent.globals.document.vars.editMode.value == "yes" ) || checkData() )
	{
		if ( msg == parent.parent.globals.document.vars.path.value )
		{
			return true;
		}
	}
	return false;
}



function checkData()
{
	if ( intlFlag != "yes" )
	{
		if ( document.forms[ 0 ].modemAreaCode.value == "" )
		{
			alert( "You must enter an area code." );
			parent.parent.globals.setFocus( document.forms[ 0 ].modemAreaCode );
			return false;
		}
		if ( parent.parent.globals.verifyAreaCode( document.forms[ 0 ].modemAreaCode.value ) == false )
		{
			alert( "Please enter a valid area code." );
			parent.parent.globals.setFocus( document.forms[ 0 ].modemAreaCode );
			return false;
		}
	}
	if ( document.forms[ 0 ].modemPhoneNumber.value == "" )
	{
		alert( "You must enter a telephone number." );
		parent.parent.globals.setFocus( document.forms[ 0 ].modemPhoneNumber );
		return false;
	}
	if ( parent.parent.globals.verifyPhoneNumber( document.forms[ 0 ].modemPhoneNumber.value ) == false )
	{
		alert( "Please enter a valid telephone number." );
		parent.parent.globals.setFocus( document.forms[ 0 ].modemPhoneNumber );
		return false;
	}
	return true;
}



function loadData()
{
	// ¥ make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
	if (	(	( document.forms[ 0 ].modemAreaCode == "undefined" ) || 
				( document.forms[ 0 ].modemAreaCode == "[object InputArray]" )
			) ||
			(	( document.forms[ 0 ].modemPhoneNumber == "undefined" ) || 
				( document.forms[ 0 ].modemPhoneNumber == "[object InputArray]" ) ) )
	{
		top.globals.debug( "FORM ELEMENT = " + document.forms[ 0 ].modemPhoneNumber );
		top.globals.debug( "SET FOCUS: " + document.forms[ 0 ].modemAreaCode + "BAD OBJECT!!!" );
		top.globals.debug( "HISTORY: " + parent.content.history );
		parent.controls.reloadDocument();
		return;
	}

	document.forms[ 0 ].modemAreaCode.value = parent.parent.globals.document.vars.modemAreaCode.value;

	if ( intlFlag != "yes" )
	{
		if ( document.forms[ 0 ].modemAreaCode.value == "" )
		{
			document.forms[ 0 ].modemAreaCode.value = parent.parent.globals.document.vars.areaCode.value;
		}
	}

	document.forms[ 0 ].modemPhoneNumber.value = parent.parent.globals.document.vars.modemPhoneNumber.value;
	if ( document.forms[ 0 ].modemPhoneNumber.value == "" )
	{
		document.forms[ 0 ].modemPhoneNumber.value = parent.parent.globals.document.vars.phoneNumber.value;
	}

	document.forms[ 0 ].altAreaCode1.value = parent.parent.globals.document.vars.altAreaCode1.value;
	document.forms[ 0 ].altAreaCode2.value = parent.parent.globals.document.vars.altAreaCode2.value;
	document.forms[ 0 ].altAreaCode3.value = parent.parent.globals.document.vars.altAreaCode3.value;
	
	parent.parent.globals.setFocus( document.forms[ 0 ].modemAreaCode );
	if ( parent.controls.generateControls )
		parent.controls.generateControls();
}



function saveData()
{
	// make sure all form element are valid objects, otherwise just skip & return!
	if (((document.forms[0].modemAreaCode == "undefined") || (document.forms[0].modemAreaCode == "[object InputArray]")) ||
		((document.forms[0].modemPhoneNumber == "undefined") || (document.forms[0].modemPhoneNumber == "[object InputArray]")))
	{
		top.globals.debug("SAVE DATA ...");
		top.globals.debug("FORM ELEMENT = " + document.forms[0].modemPhoneNumber);
		top.globals.debug("SET FOCUS: " + document.forms[0].modemAreaCode + "BAD OBJECT!!!");
		top.globals.debug("HISTORY: " + parent.content.history);
		parent.controls.reloadDocument();
		return;
	}

	if ( intlFlag != "yes" )
	{
		parent.parent.globals.document.vars.modemAreaCode.value = document.forms[ 0 ].modemAreaCode.value;
	}
	parent.parent.globals.document.vars.modemPhoneNumber.value = document.forms[ 0 ].modemPhoneNumber.value;
	parent.parent.globals.document.vars.altAreaCode1.value = document.forms[ 0 ].altAreaCode1.value;
	parent.parent.globals.document.vars.altAreaCode2.value = document.forms[ 0 ].altAreaCode2.value;
	parent.parent.globals.document.vars.altAreaCode3.value = document.forms[ 0 ].altAreaCode3.value;
}



// end hiding contents from old browsers  -->
