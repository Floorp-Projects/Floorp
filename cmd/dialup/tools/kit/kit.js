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
	
function loadData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	//get the platform
	var thePlatform = new String(navigator.userAgent);
	var x=thePlatform.indexOf("(")+1;
	var y=thePlatform.indexOf(";",x+1);
	thePlatform=thePlatform.substring(x,y);
	

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
			top.toolbar.visible=false;
			top.menubar.visible=false;
			top.locationbar.visible=false;
			top.personalbar.visible=false;
			top.statusbar.visible=false;
			top.scrollbars.visible=false;

			var screenWidth = screen.width;
			var screenHeight = screen.height;
			var windowTitleHeight=20;
			var windowFrameWidth=(7*2);
			var windowFrameHeight=(15*2);
			var menuBarHeight = 0;

			var thePlatform = new String(navigator.userAgent);
			var x=thePlatform.indexOf("(")+1;
			var y=thePlatform.indexOf(";",x+1);
			thePlatform=thePlatform.substring(x,y);
			if (thePlatform == "Macintosh")	{		// Macintosh support
				menuBarHeight = 20;					// adjust for menubar size
				screenHeight = screenHeight - menuBarHeight;
				}

			var ourWidth = 640 - windowFrameWidth;
			var ourHeight = 480 - menuBarHeight - windowFrameHeight;					
			var x = (screenWidth/2) - (ourWidth/2) - windowFrameWidth;
			var y = (screenHeight/2) - (ourHeight/2) - windowTitleHeight - windowFrameHeight;

			if (x<-3)	x=-3;
			if (y<0)	y=0;
			moveTo(x,y);
			if (ourWidth > screenWidth)		ourWidth = screenWidth;
			if (ourHeight > screenHeight)	ourHeight = screenHeight;
			resizeTo(ourWidth,ourHeight);

			window.setHotkeys(false);
			window.setResizable(false);

			navigator.preference("general.always_load_images", true);
			}
		}

	if (thePlatform == "Macintosh")
		window.location.replace("mkit.htm");
	else
		window.location.replace("wkit.htm");
		
}

// end hiding contents from old browsers  -->
