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

var				sizeArray = null;
var				showArray = null;

window.captureEvents( Event.MOUSEUP| Event.MOUSEDOWN| Event.MOUSEDRAG );

function go( msg )
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	if ( parent.parent.globals.document.vars.editMode.value != "yes" )
	{
	}
	return true;
}

function checkData()
{
	return true;
}

function loadData()
{
}

function saveData()
{
}

function savePositions()
{
	sizeArray = new Array;
	showArray = new Array;
	
	allisp = document.layers[ "allisp" ];

	for ( i = 0; i < allisp.layers.length; i++ )
	{
		sizeArray[ i ] = allisp.layers[ i ].document.height
		showArray[ i ] = true;
	}
}

function tabIndex( tab )
{
	allisp = document.layers[ "allisp" ];

	for ( i = 0; i < allisp.layers.length; i++ )
	{
		if ( tab.name == allisp.layers[ i ].name )
			return i;
	}
	return -1;
}

function toggleTab( tab )
{
	allisp = document.layers[ "allisp" ];

	if ( tab )
		index = tabIndex( tab );
	if ( index != -1 )
	{
		height = sizeArray[ index ];
		if ( showArray[ index ] == true  )
		{
			showArray[ index ] = false;
			tab.clip.bottom = tab.clip.top + 54; 
			for ( i = index + 1; i < allisp.layers.length; i++ )
			{
				layerToMove = allisp.layers[ i ];
				layerToMove.moveBy( 0, - ( height - 54 ) );
			}
		}
		else
		{
			showArray[ index ] = true; 
			for ( i = index + 1; i < allisp.layers.length; i++ )
			{
				layerToMove = allisp.layers[ i ];
				layerToMove.moveBy( 0, ( height - 54 ) );
			}
			tab.clip.bottom = height;
		}
	}
}

function toggle( tabName )
{
	allisp = document.layers[ "allisp" ];
	toggleTab( allisp.layers[ tabName ] );
}

function checkTab( x, y )
{
	allisp = document.layers[ "allisp" ];
	
	for ( var i = 0; i < allisp.layers.length; i++ )
	{
		checkLayer = allisp.layers[ i ];
		
		if (	( x >= i.pageX ) && ( x <= ( i.pageX + 300 ) ) &&
			( y >= i.pageY ) && ( y <= ( i.pageY + 16 ) ) )
			return checkLayer;
	}
	return false;
}

function checkClick( e )
{
	possibleTab = checkTab( e.pageX, e.pageY );
	if ( possibleTab )
		toggleTab( possibleTab );
}

function donothing()
{
}

window.onmousedown = checkClick;

// end hiding contents from old browsers  -->
