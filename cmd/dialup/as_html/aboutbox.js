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
	if ( msg=="Back" )
		return true;
	return false;
}



function checkData()
{
	return true;
}

function loadData()
{
	if ( parent && parent.controls && parent.controls.generateControls )
	{
		parent.controls.generateControls();
	}
	document.layers[ "programName" ].visibility = "SHOW";
	animate1();
}

function saveData()
{
}

function animate1()
{
	if ( document.layers[ "programName" ].left < 100 )
	{
		document.layers[ "programName" ].moveBy( 10, 0 );
		setTimeout( "animate1()", 100 );
	}
	else
	{
		document.layers[ "programDesc" ].visibility = "SHOW";
		animate2();
	}
}



function animate2()
{
	if ( document.layers[ "programDesc" ].top < 0 )
	{
		document.layers[ "programDesc" ].moveBy( 0, 10 );
		setTimeout( "animate2()", 100 );
	}
	else
	{
		document.layers[ "programCopyRight" ].visibility = "SHOW";
		animate3();
	}
}



function animate3()
{
	if ( document.layers[ "programCopyRight" ].left > 380 )
	{
		document.layers[ "programCopyRight" ].moveBy( -10, 0);
		setTimeout( "animate3()", 100 );
	}
	else
	{
		document.layers[ "NetscapeIcon" ].visibility = "SHOW";
		document.layers[ "ProgramThanks" ].visibility = "SHOW";
		animate4();
	}
}



function animate4()
{
	if ( document.layers[ "ProgramThanks" ].top > 50 )
	{
		document.layers[ "NetscapeIcon" ].moveBy( 0, 5 );
		document.layers[ "ProgramThanks" ].moveBy( 0, -10 );
		setTimeout( "animate4()", 100 );
	}
	else
	{
		document.layers[ "NetscapeIcon" ].document.layers[ "NetscapeString" ].visibility = "SHOW";
	}
}



// end hiding contents from old browsers  -->
