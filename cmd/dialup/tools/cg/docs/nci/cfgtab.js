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
		var FormName = "CFGFILESELECT";
		var selectedScript = "";
		
		parent.parent.nciglobals.fillSelectListWithConfigFiles(FormName, "CFGList", ".CFG", false);
		selectedScript = parent.parent.nciglobals.getGlobal("ConfigurationFileName");
		parent.parent.nciglobals.setSelectList(FormName, "CFGList", selectedScript);
		this.focus();
		document.forms[0]["CFGList"].focus();
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
		var FormName 	= "CFGFILESELECT";
		var CFGName = null;
	
		//now check if we have a valid script file selected & set the globals accordingly
		CFGName  = parent.parent.nciglobals.getSelectListSelection(FormName, "CFGList");
	
		if (CFGName != null)
			parent.parent.nciglobals.setGlobal("ConfigurationFileName", CFGName);
		else
			parent.parent.nciglobals.setGlobal("ConfigurationFileName", "");
	}
}

// end hiding contents from old browsers  -->
