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

// iasglob.js

//THE FOLLOWING FUNCTION IS LOCATION DEPENDANT

function refreshConfigFrame(fileName)
{
	debug("Globals: refreshing config frame");
	if (parent.ias)
	{
		debug("Globals: refreshing config frame : parent");
		parent.ias.refreshIASList();
		if (fileName != null)
			parent.ias.setIASList(fileName);
	} 
	else if (parent.parent.ias)
	{
		debug("Globals: refreshing config frame : parent.parent");
		parent.parent.ias.refreshIASList();
		if (fileName != null)
			parent.parent.ias.setIASList(fileName);
	}
	else
	{
		//diagnostics
			debug("WARNING: Globals:did not find IAS frame");
			debug("parent: " + parent + " ["+parent.name+"]: " + parent.ias);
			debug("parent.parent: " + parent.parent + " ["+parent.parent.name+"]: " + parent.parent.ias);
			debug("this document: " + document.location + document.name + document.ias);
	}
}



////////////////Interaction functions////////////////
// these functions are for other files to access and load the global data
// we store in the iasglobals frame

function getGlobal(fieldName)
{
	
	if (parent.parent.iasglobals)
	{
		//debug ("option 1: parent.parent.iasglobals");
		outValue = parent.parent.iasglobals.document.iasTempVars[fieldName].value;
	}
	else if (parent.iasglobals && parent.iasglobals.document && parent.iasglobals.document.iasTempVars && parent.iasglobals.document.iasTempVars[fieldName])
	{
		//debug ("option 2: parent.iasglobals");
		var outValue = parent.iasglobals.document.iasTempVars[fieldName].value;
	}

	if (outValue == null) outValue = "";
	return outValue;
}

function setGlobal(fieldName, inValue)
{
	if (inValue == null)
		inValue = "";

	if (parent.parent.iasglobals)
	{
		//debug ("SetGobal: option 1: parent.parent.iasglobals");
		parent.parent.iasglobals.document.iasTempVars[fieldName].value = inValue;
	}
	else if (parent.iasglobals)
	{
		//debug ("SetGobal: option 2: parent.iasglobals");
		parent.iasglobals.document.iasTempVars[fieldName].value = inValue;
	}

}


//call this to force a save of the data, and refresh the frames.
function saveAndRefresh()
{
	//flush the current child frame
	if (parent.tabs.tabbody)
		parent.tabs.tabbody.saveData();
	else if (parent.parent.tabs.tabbody)
		parent.tabs.tabbody.saveData();
	
	//save the file
	var fileName = saveNewOrOldFile();
	
	//refresh the popup
	if ((fileName != null) && (fileName != false))
		refreshConfigFrame(fileName);
}

//specifies whether the data has been tainted - i.e. whether we should 
// save the data on unload
// usage: 	iasDirty(true) sets dirtiness (returns old dirtiness state)
//			iasDirty(false) clears dirtiness (returns old dirtiness state)
//			iasDirty(null) returns current dirtiness state
function iasDirty(inDirt)
{
	var isDirty = getGlobal("GlobalsDirty");
	
	if ((inDirt != null) && (inDirt == true))
		setGlobal("GlobalsDirty", "yes");
	else if ((inDirt != null) && (inDirt == false))
		setGlobal("GlobalsDirty", "no");
		
	if ((isDirty != null) && (isDirty == "yes"))
		return true;
	else
		return false;
}


/////////////End Interaction functions////////////////



////////FILE I/O FUNCTIONS/////////


//get all the data from some file in the Config folder
//fill in the values in our globals form
function readFileData(fileName)
{
	var saved = saveIfDirty();
	
	debug("saved : " + saved);

	if (saved == false)
	{
		var siteName = getGlobal("IASFileName");
		if ((siteName == null) || (siteName == "") || (siteName == "null"))
		{
			siteName = "_new_";
		}
		refreshConfigFrame(siteName);
		return false;
	}
	
	
	if ((saved != null) && (saved != "") && (saved != false))
	{
		refreshConfigFrame(fileName);
	}

	var tempFlagValue 		= 	"";
	
	var dicSectionName 		= 	"Dial-In Configuration";
	var ipSectionName		=	"IP";
	var securitySectionName	=	"Security";				

	var configurationSectionName	=	"Configuration";	//this is new - to take up some of the slack
															//from reg.ini
	//old legacy stuff
	//var ipxSectionName		=	"IPX";
	//var netbeuiSectionName	=	"NetBEUI";
	//var windowsSectionName	=	"Windows";

	if (fileName == null) fileName = "";

	setGlobal("IASFileName", fileName);
	debug("just set Global filename to: " + fileName);


	tempFlagValue = GetNameValuePair(fileName, dicSectionName, "SiteName");
	setGlobal("SiteName", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, dicSectionName, "Description");
	setGlobal("Description", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, dicSectionName, "Name");
	setGlobal("Name", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, dicSectionName, "Password");
	setGlobal("Password", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, dicSectionName, "Phone");
	setGlobal("Phone", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, dicSectionName, "ScriptFileName");
	setGlobal("ScriptFileName", tempFlagValue);


	tempFlagValue = GetNameValuePair(fileName, ipSectionName, "Enabled");
	setGlobal("IPEnabled", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, ipSectionName, "IPAddress");
	setGlobal("IPAddress", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, ipSectionName, "DomainName");
	setGlobal("DomainName", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, ipSectionName, "DNSAddress");
	setGlobal("DNSAddress", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, ipSectionName, "DNSAddress2");
	setGlobal("DNSAddress2", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, ipSectionName, "RegCGI");
	setGlobal("RegCGI", tempFlagValue);

	tempFlagValue = GetNameValuePair(fileName, configurationSectionName, "REG_SCRIPTING");
	setGlobal("REG_SCRIPTING", tempFlagValue);

	tempFlagValue = GetNameValuePair(fileName, securitySectionName, "SecurityDevice");
	setGlobal("SecurityDevice", tempFlagValue);

	//old legacy stuff
	//tempFlagValue = GetNameValuePair(fileName, ipxSectionName, "Enabled");
	//setGlobal("IPXEnabled", tempFlagValue);	
	//tempFlagValue = GetNameValuePair(fileName, netbeuiSectionName, "Enabled");
	//setGlobal("NetBEUIEnabled", tempFlagValue);
	//tempFlagValue = GetNameValuePair(fileName, windowsSectionName, "Main");
	//setGlobal("Main", tempFlagValue);
	
	//set the flag in ACCTSET.INI to indicate THIS is the new file
	if (fileName == null) fileName = "";
	
	if ((fileName != null) && (fileName != "_new_"))
	SetNameValuePair("ACCTSET.INI", "New Acct Mode", "RegServer", fileName);

	//set the dirty bit to clean
	iasDirty(false);
	
	//reload the parent's tabs frame
	//debug("parent.tabs: " + parent.tabs);
	//debug("parent.parent.tabs: " + parent.parent.tabs);
	
	if (parent.tabs && parent.tabs.tabbody && parent.tabs.tabbody.loadData) 
		parent.tabs.tabbody.loadData();
		

	return true;
}

function writeToFile(fileName)
{
	//alert("in writeToFile");

	var tempFlagValue 		= 	"";
	
	var dicSectionName 		= 	"Dial-In Configuration";
	var ipSectionName		=	"IP";
	var securitySectionName	=	"Security";				

	var configurationSectionName	=	"Configuration";	//this is new - to take over REG.INI stuff

	//old legacy stuff
	//var ipxSectionName		=	"IPX";
	//var netbeuiSectionName	=	"NetBEUI";
	//var windowsSectionName	=	"Windows";

	
	SetNameValuePair(fileName, dicSectionName, "SiteName", getGlobal("SiteName"));
	SetNameValuePair(fileName, dicSectionName, "Description", getGlobal("Description"));
	SetNameValuePair(fileName, dicSectionName, "Name", getGlobal("Name"));
	SetNameValuePair(fileName, dicSectionName, "Password", getGlobal("Password"));
	SetNameValuePair(fileName, dicSectionName, "Phone", getGlobal("Phone"));
	SetNameValuePair(fileName, dicSectionName, "ScriptFileName", getGlobal("ScriptFileName"));

	SetNameValuePair(fileName, ipSectionName, "Enabled", getGlobal("IPEnabled"));
	SetNameValuePair(fileName, ipSectionName, "IPAddress", getGlobal("IPAddress"));
	SetNameValuePair(fileName, ipSectionName, "DomainName", getGlobal("DomainName"));
	SetNameValuePair(fileName, ipSectionName, "DNSAddress", getGlobal("DNSAddress"));
	SetNameValuePair(fileName, ipSectionName, "DNSAddress2", getGlobal("DNSAddress2"));
	SetNameValuePair(fileName, ipSectionName, "RegCGI", getGlobal("RegCGI"));

	SetNameValuePair(fileName, configurationSectionName, "REG_SCRIPTING", getGlobal("REG_SCRIPTING"));
	SetNameValuePair(fileName, securitySectionName, "SecurityDevice", getGlobal("SecurityDevice"));

	//old legacy stuff
	//SetNameValuePair(fileName, ipxSectionName, "Enabled", getGlobal("IPXEnabled"));
	//SetNameValuePair(fileName, netbeuiSectionName, "Enabled", getGlobal("NetBEUIEnabled"));
	//SetNameValuePair(fileName, windowsSectionName, "Main", getGlobal("Main"));
	//SetNameValuePair(fileName, ipSectionName, "DNSAddress2", getGlobal("DNSAddress2"));
	
	iasDirty(false);	
}


////////////FILE NAMING FUNCTIONS/////////////////
//helper function for suggesting fileNames
function isAlphaNumeric(inLetter)
{

	//debug("inAlphaNumeric");
	var outValue = false;
	
	if ((inLetter != null) && (inLetter != ""))
	{

		if ((!isNaN(parseInt(inLetter))) && (parseInt(inLetter >= 0)) && (parseInt(inLetter <=9)))
			outValue = true;
		else
		{
			var let = inLetter.toLowerCase();
			if ((let <= "z") && ( let >= "a"))
				outValue = true;
			else if (let == "_")
				outValue = true;
		}
	}
	return outValue;
}

//takes the first eight non-whitespace characters of SiteName, and turns them into a filename
//ending with a .IAS
function suggestIASFileName(sName)
{	
	//debug("suggesting");
	if ((sName == null) || (sName == ""))
		sName 	= 	getGlobal("SiteName");

	var iasName =	"";
	
	if ((sName != null) && (sName != ""))
	{
		var len 	= 	sName.length;
		var finLen	=	0;
		
		for (var i = 0; i < len; i++)
		{
			if (isAlphaNumeric(sName.charAt(i)))
			{
				iasName = iasName + sName.charAt(i);
				finLen++;
			}
			if (finLen >= 8)
				break;
		}
		iasName = (iasName + ".IAS"); 			
	}
	return iasName;
}


function checkIfIASFileExists(inFileName)
{
	var outValue = false;
	
	if ((inFileName != null) && (inFileName != ""))
	{	
		fileList = getFileListFromConfigFolder(".IAS");
		
		if (fileList != null)	
		{
			for (var i=0; i<fileList.length; i++)	
			{
				if (fileList[i] == inFileName)
					outValue = true;
			}
		}
	}
	return outValue;
}

////////////////FILE SAVING FUNCTIONS///////////////
function askIASFileNameAndSave()
{
	var sName 	= 	getGlobal("SiteName");
	var save 	= 	null;
	

	//flush data from currenly open tab into globals
	if (parent.tabs && parent.tabs.tabbody && parent.tabs.tabbody.saveData)
	{
		parent.tabs.tabbody.saveData();
	}

	//first check if there is a sitename, if not, prompt for one
	if ((sName == null) || (sName == ""))
	{
		var pName = prompt("Please enter a name for this configuration (this will be shown to the end user):", "");
		if ((pName != null) && (pName != ""))
		{
			setGlobal("SiteName", pName);
			setGlobal("Description", pName);
			sName = pName;
		}
		else
		{
			save = false;
		}	
	}
	
	//now, if there is a sitename, suggest an IAS fileName, prompt for confirmation
	//debug( "about to suggest..." );
	while (save == null)
	{
		//debug( "in that while loop..." );
		if ((sName != null) && (sName != ""))
		{
			var sgName = getGlobal("IASFileName");
		
			if ((sgName == null) || (sgName == "") || (sgName == "_new_"))
				sgName = suggestIASFileName(null);
			
			var fName = prompt("Enter the file name for this configuration (must end with .IAS)", sgName);
			
			//if they entered an improper suffix, prompt again, and again
			while ((fName != null) && (fName.substring(fName.length-4, fName.length)  != ".IAS"))
			{
				sgName = suggestIASFileName(fName);
				fName = prompt("Enter the fileName for this configuration (must end with .IAS)", sgName);
			}
			
			// if the name exists, prompt to replace
			
			if ((fName != null) && (checkIfIASFileExists(fName)))
			{
				var conf = confirm("The file: " + fName + " already exists.  Replace?");
				if (conf)
				{ 
					save = true;
				}
				else
					save = null;
			}
			else if(fName != null)
			{
				//fileName doesn't exist - we can save
				save = true;
			}
			else
			{
				//user canceled - name = null
				save = false;		
			}
		}
	}	
	
	//now save the file if the user didn't cancel
	if (save == true)
	{
		//save the file
		writeToFile(fName);
		refreshConfigFrame(fName);
		return fName;
	}
	else
	{
		//alert("Could not save this configuration because you did not provide a Name");
		return null;
	}
}



//EVENT HANDLERS

function loadData()
{
	if (parent.ias && parent.ias.loadData)
	{
		parent.ias.loadData();
	
		//generate toolbar controls (and load help file if needed)
		if (parent.parent && parent.parent.controls && parent.parent.controls.generateToolBarControls)
			parent.parent.controls.generateToolBarControls();
	}
	else
		setTimeout("loadData()",500);
	//debug("in getdata of iasglob.js");
}

//checks validity of entered data to see if we're ready to unload
function checkData()
{
	if (iasDirty(null))
	{
		if (parent.tabs.tabbody.checkData)
		{
			//alert("checkdata exists");
			var checkResult = parent.tabs.tabbody.checkData();
			if (checkResult == false)
				return checkResult;
		}
		else
			return true;
	}
	else 
		return true;
}

//checks to see if globals are dirty - if so, asks if you wanna save
function saveIfDirty()
{

	//alert("in SaveifDirty, caller: " + saveIfDirty.caller);
	var fName=null;
		
	if (iasDirty(null))
	{
		if (confirm("You have made changes to this configuration.  Would you like to save them?"))		
			fName= saveNewOrOldFile();
		//alert("fName: " + fName);
		if (fName != false)
			iasDirty(false);
		else
			return false;
	}
	//else
		//debug("Won't save - IAS not dirty");
	if ((fName != null) && (fName != ""))
		return fName;
	else 
		return null;	
}


// checks if fileName exists, if not asks about saving to a new file, 
// else saves over an old file (no prompting)
function saveNewOrOldFile()
{

	//alert("in SavenewOrOldFile");
	//first checkData
	var fileName = checkData();
	if (fileName == false)
		return fileName;


	//flush data from currenly open tab into globals
	if(parent.tabs && parent.tabs.tabbody && parent.tabs.tabbody.saveData)
	{
		parent.tabs.tabbody.saveData();
	}

	iasDirty(false);

	fileName = getGlobal("IASFileName");
		
	if ((fileName == null) || (fileName == "") || (fileName == "_new_"))
	{
		//debug("Saving: must ask for FileName");
		var theName = askIASFileNameAndSave();
		fileName = theName;
	}
	else if ((fileName != null) && (fileName != "") && (fileName != "_new_"))
	{
		//debug("Saving: without asking to: " + fileName);
		writeToFile(fileName);
	}
	return fileName;
}


//this is the onUnLoad event Handler
function saveData()
{
	//alert("in saveData");
	//do a checkdata
	var checkResult = checkData();
	if (checkResult == false)
		return false;

	//first make sure any current changes in the open tab are reflected in the globals
	if(parent.tabs && parent.tabs.tabbody && parent.tabs.tabbody.saveData)
	{
		parent.tabs.tabbody.saveData();
	}
	
		saveIfDirty();	//save, if need be
	return true;
}

// end hiding contents from old browsers  -->
