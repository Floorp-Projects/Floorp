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
	
// Request privilege

netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );
compromisePrincipals();						// work around for the security check
enableExternalCapture(); 					// This requires UniversalBrowserWrite access
parent.captureEvents(Event.MOUSEDOWN | Event.MOUSEUP | Event.DRAGDROP | Event.DBLCLICK);
parent.onmousedown = cancelEvent;
parent.onmouseup = cancelEvent;
parent.ondragdrop = cancelEvent;

function cancelEvent( e )
{
	var retVal = false;

	if ( ( e.which < 2 ) && ( e.type != "dragdrop" ) && ( e.type != "dblclick" ) )
		retVal = routeEvent( e );
	return retVal;
}

window.setHotkeys( false );

function loadData()
{
	var theRef = opener.document.forms[ 0 ].packageRef.value;
	document.forms[ 0 ].packageRef.value = theRef;
	opener.top.location = "../../" + theRef + "/start.htm";
}

// end hiding contents from old browsers  -->
