/* 
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):
 *    Sammy Ford 
 */


function StartupViewSource()
{
	// Create and initialize the browser instance.
	createBrowserInstance();

	if ( appCore ) {
	    appCore.isViewSource = true;
	    appCore.setContentWindow(window.frames[0]);
	    appCore.setWebShellWindow(window);
	    appCore.setToolbarWindow(window);
	}

	// Get url whose source to view.
	var url = window.arguments[0];

	// Load the source (the app core will magically know what to do).
	appCore.loadUrl( url );
}
