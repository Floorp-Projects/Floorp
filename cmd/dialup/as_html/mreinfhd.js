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

function loadData()
{
	var		fileToLoad = "isp/en/" + parent.parent.parent.globals.selectedISP +
				"/client_data/html/sales.html";
	
	parent.moreinfo.location.replace( fileToLoad );
	
	if ( parent.parent.controls.generateControls )
		parent.parent.controls.generateControls();
}

function generateHeader()
{
//	parent.parent.parent.globals.debug( "ispDisplayName" + ispDisplayName );

	document.writeln( "More Information about " + 
		parent.parent.parent.globals.getSelectedISPName() );
}

// end hiding contents from old browers -->
