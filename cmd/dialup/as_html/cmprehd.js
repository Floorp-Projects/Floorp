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

var globals = parent.parent.parent.globals;

function loadData()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );
	
	globals.debug( "removing support window" );
	
	if ( globals.supportWindow && globals.supportWindow != null )
	{
		globals.supportWindow.close();
		globals.supportWindow = null;
	}

	if ( parent.parent.controls.generateControls )
		parent.parent.controls.generateControls();
}

// end hiding contents from old browers -->
