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

//the file that includes this must also include settings.js and nciglob.js
var loaded = false;

function loadData()
{
	if (parent.parent.nciglobals && parent.parent.nciglobals.getText)
	{
		var FormName = "NCIPUBLISH";
		
		
		parent.parent.nciglobals.setText(FormName, "VIEWURL", parent.parent.nciglobals.getGlobal("View_URL"));			
		
		parent.parent.nciglobals.setText(FormName, "PUBLISHURL", parent.parent.nciglobals.getGlobal("Publish_URL"));			
		parent.parent.nciglobals.setText(FormName, "PUBLISHPASS", parent.parent.nciglobals.getGlobal("Publish_Password"));			

		this.focus();
		document.forms[0]["PUBLISHURL"].focus();
		loaded = true;
	}
	else
		setTimeout("loadData()",500);
}//function loadData()


function checkData()
{
	return true;
}


function saveData()
{
	if (loaded && loaded == true)
	{
		var FormName = "NCIPUBLISH";
		
		parent.parent.nciglobals.setGlobal("View_URL", parent.parent.nciglobals.getText(FormName, "VIEWURL"));
		parent.parent.nciglobals.setGlobal("Publish_URL", parent.parent.nciglobals.getText(FormName, "PUBLISHURL"));
		parent.parent.nciglobals.setGlobal("Publish_Password", parent.parent.nciglobals.getText(FormName, "PUBLISHPASS"));
	}
}




// end hiding contents from old browsers  -->
