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

// the file that includes this must also include settings.js!

//THE FOLLOWING FUNCTION IS LOCATION DEPENDANT
//returns the filename that we are working with - from the 
//editfield in the parent's "config" frame
function getNCINameFromOpener()
{
	var outValue = "";
	
	var tempValue = parent.nciglobals.GetNameValuePair("ACCTSET.INI", "Mode Selection", "ExistingSRFile");
	if ((tempValue != null))
		outValue = tempValue;
	return outValue;
	
}


function refreshNCIList()
{
	formName = "NCICONFIGSELECT";

	fillSelectListWithConfigFiles(formName, "NCICONFIGLIST", ".NCI", true);
}

function setNCIList(inFileName)
{
	formName = "NCICONFIGSELECT";
	setSelectList(formName, "NCICONFIGLIST", inFileName);
}

//this is the onLoad event handler
function loadData()
{
	if (parent.nciglobals && parent.nciglobals.getText)
	{
		refreshNCIList();
	
		var openerConfigName = getNCINameFromOpener();
		//debug("Opener Config Name: " + openerConfigName);
		if ((openerConfigName != null) && (openerConfigName != ""))
		{
			setNCIList(openerConfigName);
			parent.nciglobals.readFileData(openerConfigName);
		}
	}
	else
		setTimeout("loadData()",500);
	
}

function saveData()
{
	//do nothing
}

function setSelectList(formName, selectListName, inValue)
{
	if (inValue != null)
	{ 
	 	for (var i = (document[formName][selectListName].length - 1); i >= 0 ; i--)
	 	{
	 		if(inValue.toString() == (document[formName][selectListName].options[i].value.toString()))
	 		{
	 			//debug("Found select list Match (" + i + ")(" + inValue + ")");
	 			document[formName][selectListName].options[i].selected = true;
	 		}
	 	}
	}	
}

function fillSelectListWithConfigFiles(formName, selectListName, fileSuffix, offerNew)
{
	var curConfigName = "";
	var curConfigDescription = "";

	//blank out old list
	for (var i = (document[formName][selectListName].length -1); i >= 0 ; i--)
	{
		document[formName][selectListName].options[i] = null;
	}	
	
	//make a blank so the user can choose nothing
	document[formName][selectListName].options[0] = new Option("< None Selected >","",false, false); 
	
	//Only offer a new if we said so
	if (offerNew == true)
		document[formName][selectListName].options[1] = new Option("< New Configuration >","_new_",false, false); 

	// Get a list of the files in the Config folder
	fileList = parent.nciglobals.getFileListFromConfigFolder(fileSuffix);
	
	//debug( "filelist: " + fileList);
	
	if (fileList != null)	{
		for (var i=0; i<fileList.length; i++)	{
				curConfigName 			= parent.nciglobals.GetNameValuePair(fileList[i], "Dial-In Configuration", "SiteName");
				curConfigDescription 	= parent.nciglobals.GetNameValuePair(fileList[i], "Dial-In Configuration", "Description");

				if ((curConfigName != null)  && (curConfigName != ""))
					document[formName][selectListName].options[document[formName][selectListName].length] = new Option(curConfigName,fileList[i], false, false);
				else if ((curConfigDescription != null) && (curConfigDescription != ""))
					document[formName][selectListName].options[document[formName][selectListName].length] = new Option(curConfigDescription,fileList[i], false, false);
				else
					document[formName][selectListName].options[document[formName][selectListName].length] = new Option(fileList[i],fileList[i], false, false);
					
			}
		}
}




// end hiding contents from old browsers  -->
