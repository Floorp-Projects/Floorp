/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

var progressMeter = 0;
var progressInfo = null;
var progressStatus = null;

function OnLoadProgressDialog()
{
	
	
	dump( "*** Loaded progress window\n");

	top.progressMeter = document.getElementById( 'progressMeter');
	top.progressStatus = document.getElementById( 'progressStatus');

	// look in arguments[0] for parameters
	if (window.arguments && window.arguments[0])
	{
		// keep parameters in global for later
		if ( window.arguments[0].windowTitle )
			top.window.title = window.arguments[0].windowTitle;
		
		if ( window.arguments[0].progressTitle )
			document.getElementById( 'progressTitle').setAttribute( "value", window.arguments[0].progressTitle);
		
		if ( window.arguments[0].progressStatus )
			top.progressStatus.setAttribute( "value", window.arguments[0].progressStatus);
		
		top.progressInfo = window.arguments[0].progressInfo;
	}

	top.progressInfo.progressWindow = top.window;
	top.progressInfo.intervalState = setInterval( "Continue()", 100);

}

function SetStatusText( val)
{
	top.progressStatus.setAttribute( "value", val);
}

function SetProgress( val)
{
	top.progressMeter.setAttribute( "value", val);
}

function Continue()
{
	top.progressInfo.mainWindow.ContinueImport( top.progressInfo);
}

