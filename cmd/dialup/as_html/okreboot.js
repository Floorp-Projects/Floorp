/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
<!--  to hide script contents from old browsers

function go( msg )
{
	if ( parent.parent.globals.document.vars.editMode.value == "yes" )
		return true;
	else
		return checkData();
}

function checkData()
{
	// check browser version
	var theAgent=navigator.userAgent;
	var x=theAgent.indexOf("/");
	if (x>=0)	{
		theAgent=theAgent.substring(x+1,theAgent.length);
		x=theAgent.indexOf(".");
		if (x>0)	{
			theAgent=theAgent.substring(0,x);
			}			
		if (parseInt(theAgent)>=4)	{
		
			// Navigator 4.x specific features
		
			top.toolbar=true;
			top.menubar=true;
			top.locationbar=true;
			top.directory=true;
			top.statusbar=true;
			top.scrollbars=true;
			}
		}

	return(true);
}

function loadData()
{
	if ( parent.controls.generateControls )
		parent.controls.generateControls();
	if ( parent.parent.globals.document.vars.editMode.value != "yes" )
		saveAccountInfo( false );
}

function saveData()
{
}

var savedFlag = false;

function saveAccountInfo( promptFlag )
{
	savedFlag = parent.parent.globals.saveAccountInfo( promptFlag );
}

// end hiding contents from old browsers  -->
