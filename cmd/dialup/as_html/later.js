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



function go(msg)
{
	return(checkData());
}



function checkData()
{
	// check browser version
	var theAgent=navigator.userAgent;
	var x=theAgent.indexOf("/");
	if (x>=0)	{
		theVersion=theAgent.substring(x+1,theAgent.length);
		x=theVersion.indexOf(".");
		if (x>0)	{
			theVersion=theVersion.substring(0,x);
			}			
		if (parseInt(theVersion)>=4)	{
			if (theAgent.indexOf("4.0b2")>=0)	{
				// Navigator 4.0b2 specific features

				toolbar=true;
				menubar=true;
				locationbar=true;
				directory=true;
				statusbar=true;
				scrollbars=true;
				}
			else	{
				// Navigator 4.0b3 and later features

				toolbar.visible=true;
				menubar.visible=true;
				locationbar.visible=true;
				personalbar.visible=true;				// was directory
				statusbar.visible=true;
				scrollbars.visible=true;
				}
			}
		}

	return(true);
}



function loadData()
{
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
}



// end hiding contents from old browsers  -->
