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

// nciglob.js

//THE FOLLOWING FUNCTION IS LOCATION DEPENDANT

function refreshConfigFrame(fileName)
{
	if (parent.nci)
	{
		parent.nci.refreshNCIList();
		if (fileName != null)
			parent.nci.setNCIList(fileName);
	} 
	else if (parent.parent.nci)
	{
		parent.parent.nci.refreshNCIList();
		if (fileName != null)
			parent.parent.nci.setNCIList(fileName);
	
	}
}

//THE FOLLOWING FUNCTION IS LOCATION DEPENDANT
//returns the filename that we are working with - from the 
//editfield in the parent's "config" frame
//NOT any more, now that we run inside a window, we get the name from 
//ACCTSET.INI
function getNCINameFromOpener()
{
	var outValue = "";
	var tempValue = GetNameValuePair("ACCTSET.INI", "Mode Selection", "ExistingSRFile");
	if ((tempValue != null))
		outValue = tempValue;
	return outValue;
}


////////////////Interaction functions////////////////
// these functions are for other files to access and load the global data
// we store in the nciglobals frame

function getGlobal(fieldName)
{
	var outValue = "";
	//debug("parent.nciglobals: " + parent.nciglobals);
	//debug("parent.parent.nciglobals: " + parent.parent.nciglobals);
	//debug("parent.parent.parent.nciglobals: " + parent.parent.parent.nciglobals);
	//debug("parent.parent.parent.parent.nciglobals: " + parent.parent.parent.parent.nciglobals);
	if (parent.parent.nciglobals)
	{
		//debug ("option 1: parent.parent.nciglobals " + fieldName);
		outValue = parent.parent.nciglobals.document.nciTempVars[fieldName].value;
		//debug("out: " + outValue);
	}
	else if (parent.nciglobals && parent.nciglobals.document && parent.nciglobals.document.nciTempVars && parent.nciglobals.document.nciTempVars[fieldName])
	{
		//debug ("option 2: parent.nciglobals");
		outValue = parent.nciglobals.document.nciTempVars[fieldName].value;
	}
	else
	{
		//debug("getGlobal - Warning: NCIGLOBALS not found.");
		//debug("parent.nciglobals: " + parent.nciglobals);
		//debug("parent.parent.nciglobals: " + parent.parent.nciglobals);
		//debug("parent.parent.parent.nciglobals: " + parent.parent.parent.nciglobals);
		//debug("parent.parent.parent.parent.nciglobals: " + parent.parent.parent.parent.nciglobals);	
	}
	
	if (outValue == null) outValue = "";
		debug("getGlobal: " + fieldName + " is " + outValue);

	return outValue;
	
}


function setGlobal(fieldName, inValue)
{
	//alert("setGlobal " + fieldName + " " +inValue);
	if (inValue == null)
		inValue = "";

	if (parent.parent.nciglobals)
	{
		//debug ("SetGobal: option 1: parent.parent.nciglobals");
		parent.parent.nciglobals.document.nciTempVars[fieldName].value = inValue;
	}
	else if (parent.nciglobals)
	{
		//debug ("SetGobal: option 2: parent.nciglobals");
		parent.nciglobals.document.nciTempVars[fieldName].value = inValue;
	}
	else
	{
		debug("setGlobal - Warning: NCIGLOBALS not found.");
		debug("parent.nciglobals: " + parent.nciglobals);
		debug("parent.parent.nciglobals: " + parent.parent.nciglobals);
		debug("parent.parent.parent.nciglobals: " + parent.parent.parent.nciglobals);
		debug("parent.parent.parent.parent.nciglobals: " + parent.parent.parent.parent.nciglobals);	
	}
}

//call this to force a save of the data, and refresh the frames.
function saveAndRefresh()
{
	//alert("in saveandrefresh");
	//flush the current child frame
	if (parent.tabs && parent.tabs.tabbody && parent.tabs.tabbody.saveData)
		parent.tabs.tabbody.saveData();
	else if (parent.parent.tabs.tabbody)
		parent.tabs.tabbody.saveData();
	
	//save the file
	var fileName = saveNewOrOldFile();
	
	//refresh the popup
	if ((fileName != null))
		refreshConfigFrame(fileName);
}

//specifies whether the data has been tainted - i.e. whether we should 
// save the data on unload
// usage: 	nciDirty(true) sets dirtiness (returns old dirtiness state)
//			nciDirty(false) clears dirtiness (returns old dirtiness state)
//			nciDirty(null) returns current dirtiness state
function nciDirty(inDirt)
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
	//alert("in readFileData");
	var saved = saveIfDirty();
	
	debug("saved : " + saved);
	
	if (saved == false)
	{
		var siteName = getGlobal("NciFileName");
		if ((siteName == null) || (siteName == "") || (siteName == "null"))
		{
			siteName = "_new_";
		}
		refreshConfigFrame(siteName);
		return false;
	}
	
	if ((saved != null) && (saved != "") && saved != false)
	{
		refreshConfigFrame(fileName);
	}

	
	var tempFlagValue 		= 	"";
	
	var dicSectionName 		= 	"Dial-In Configuration";
	var optionsSectionName	=	"Options"
	var scriptSectionName	=	"Script";
	var servicesSectionName	=	"Services";
	var ipSectionName		=	"IP";

	//if we decide to support per POP CFG Files, uncomment these lines
	var configurationSectionName	= 	"Configuration"; 
	var publishingSectionName =	"Publishing";

	//var ipxSectionName		=	"IPX";
	//var netbeuiSectionName	=	"NetBEUI";
	//var securitySectionName	=	"Security";				
	
	if (fileName == null) fileName = "";
	
	setGlobal("NciFileName", fileName);

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
	tempFlagValue = GetNameValuePair(fileName, dicSectionName, "SupportPhone");
	setGlobal("SupportPhone", tempFlagValue);

	tempFlagValue = GetNameValuePair(fileName, optionsSectionName, "EnableVJCompression");
	setGlobal("EnableVJCompression", tempFlagValue);


//	Nope, don't need these 3.0 things anymore
//
//	tempFlagValue = GetNameValuePair(fileName, ipxSectionName, "Enabled");
//	setGlobal("IPXEnabled", tempFlagValue);

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
	
	// don't _want_ these, in here for legacy reasons - so old files still work
	//actually - we do want them now.
	tempFlagValue = GetNameValuePair(fileName, servicesSectionName, "SMTP_Server");
	setGlobal("SMTP_Server", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, servicesSectionName, "NNTP_Server");
	setGlobal("NNTP_Server", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, servicesSectionName, "POP_Server");
	setGlobal("POP_Server", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, servicesSectionName, "IMAP_Server");
	setGlobal("IMAP_Server", tempFlagValue);
	
	//Maybe we will want this in the near future:
	tempFlagValue = GetNameValuePair(fileName, servicesSectionName, "Default_Mail_Protocol");
	setGlobal("Default_Mail_Protocol", tempFlagValue);
	

	tempFlagValue = GetNameValuePair(fileName, publishingSectionName, "Publish_URL");
	setGlobal("Publish_URL", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, publishingSectionName, "Publish_Password");
	setGlobal("Publish_Password", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, publishingSectionName, "View_URL");
	setGlobal("View_URL", tempFlagValue);


//	these are things 3.0 used to support that we don't, and things
//	that I thought we might want to support, but currently don't
//
//	setLDAPServers(fileName);
//	
//
//	tempFlagValue = GetNameValuePair(fileName, netbeuiSectionName, "Enabled");
//	setGlobal("NetBEUIEnabled", tempFlagValue);
//
//	tempFlagValue = GetNameValuePair(fileName, securitySectionName, "SecurityDevice");
//	setGlobal("SecurityDevice", tempFlagValue);

	tempFlagValue = GetNameValuePair(fileName, scriptSectionName, "ScriptEnabled");
	setGlobal("ScriptEnabled", tempFlagValue);
	tempFlagValue = GetNameValuePair(fileName, scriptSectionName, "ScriptFileName");
	setGlobal("ScriptFileName", tempFlagValue);
	
	//if we decide to support per POP CFG Files, uncomment these lines
	tempFlagValue = GetNameValuePair(fileName, configurationSectionName, "ConfigurationFileName");
	setGlobal("ConfigurationFileName", tempFlagValue);


	//get the flag from ACCTSET.INI
	tempFlagValue = GetNameValuePair("ACCTSET.INI", "Existing Acct Mode", "RegPodURL");
	setGlobal("RegPodURL", tempFlagValue);

	//set the flag in ACCTSET.INI to indicate THIS is the new file
	if (fileName != "_new_")
		SetNameValuePair("ACCTSET.INI", "Mode Selection", "ExistingSRFile", fileName);

	//set the dirty bit to clean
	nciDirty(false);
	
	//reload the parent's tabs frame
	debug("parent.tabs: " + parent.tabs);
	debug("parent.parent.tabs: " + parent.parent.tabs);

	if (parent.tabs && parent.tabs.tabbody && parent.tabs.tabbody.loadData) 
		parent.tabs.tabbody.loadData();
	else if (parent.parent.tabs && parent.parent.tabs.tabbody && parent.parent.tabs.tabbody.loadData)
		parent.parent.tabs.tabbody.loadData();
		
	return true;
}

////////////FILE NAMING FUNCTIONS/////////////////

//helper function for suggesting fileNames
function isAlphaNumeric(inLetter)
{

	debug("inAlphaNumeric");
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
//ending with a .NCI
function suggestNCIFileName(sName)
{
	
	debug("suggesting");
	if ((sName == null) || (sName == ""))
		sName 	= 	getGlobal("SiteName");

	var nciName =	"";
	
	if ((sName != null) && (sName != ""))
	{
		var len 	= 	sName.length;
		var finLen	=	0;
		
		for (var i = 0; i < len; i++)
		{
			if (isAlphaNumeric(sName.charAt(i)))
			{
				nciName = nciName + sName.charAt(i);
				finLen++;
			}
			if (finLen >= 8)
				break;
		}
		nciName = (nciName + ".NCI"); 			
	}
	return nciName;
}


function checkIfNCIFileExists(inFileName)
{
	var outValue = false;
	
	if ((inFileName != null) && (inFileName != ""))
	{	
		fileList = getFileListFromConfigFolder(".NCI");
		
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

function askNCIFileNameAndSave()
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
	
	//now, if there is a sitename, suggest an NCI fileName, prompt for confirmation
	debug( "about to suggest..." );
	while (save == null)
	{
		debug( "in that while loop..." );
		if ((sName != null) && (sName != ""))
		{
			var sgName = getGlobal("NciFileName");
			if ((sgName == null) || (sgName == "") || (sgName == "_new_"))
				sgName = suggestNCIFileName(null);

			var fName = prompt("Enter the file name for this configuration (must end with .NCI)", sgName);
			
			//if they entered an improper suffix, prompt again, and again
			while ((fName != null) && (fName.substring(fName.length-4, fName.length)  != ".NCI"))
			{
				sgName = suggestNCIFileName(fName);
				fName = prompt("Enter the fileName for this configuration (must end with .NCI)", sgName);
			}
			
			// if the name exists, prompt to replace
			
			if ((fName != null) && (checkIfNCIFileExists(fName)))
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


//writes all data to an existing file
function writeToFile(fileName)
{
	
	// Request privilege
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var dicSectionName 		= 	"Dial-In Configuration";
	var optionsSectionName	=	"Options"
	var servicesSectionName	=	"Services";
	var scriptSectionName	=	"Script";
	var ipSectionName		=	"IP";
	
	//if we decide to support per POP CFG Files, uncomment these lines
	var configurationSectionName	= 	"Configuration"; 
	
	//var ipxSectionName		=	"IPX";
	//var netbeuiSectionName	=	"NetBEUI";
	//var securitySectionName	=	"Security";				
	var publishingSectionName =	"Publishing";

	SetNameValuePair(fileName, dicSectionName, "SiteName", getGlobal("SiteName"));
	SetNameValuePair(fileName, dicSectionName, "Description", getGlobal("Description"));
	SetNameValuePair(fileName, dicSectionName, "Name", getGlobal("Name"));
	SetNameValuePair(fileName, dicSectionName, "Password", getGlobal("Password"));
	SetNameValuePair(fileName, dicSectionName, "Phone", getGlobal("Phone"));
	SetNameValuePair(fileName, dicSectionName, "SupportPhone", getGlobal("SupportPhone"));

	SetNameValuePair(fileName, optionsSectionName, "EnableVJCompression", getGlobal("EnableVJCompression"));

	//Obselete
	//SetNameValuePair(fileName, ipxSectionName, "Enabled", getGlobal("IPXEnabled"));

	SetNameValuePair(fileName, ipSectionName, "Enabled", getGlobal("IPEnabled"));
	SetNameValuePair(fileName, ipSectionName, "IPAddress", getGlobal("IPAddress"));
	SetNameValuePair(fileName, ipSectionName, "DomainName", getGlobal("DomainName"));
	SetNameValuePair(fileName, ipSectionName, "DNSAddress", getGlobal("DNSAddress"));
	SetNameValuePair(fileName, ipSectionName, "DNSAddress2", getGlobal("DNSAddress2"));
	
	//don't really want to keep these around, just here for backwards compatibility,
	// they are not exposed in the UI
	SetNameValuePair(fileName, servicesSectionName, "SMTP_Server", getGlobal("SMTP_Server"));
	SetNameValuePair(fileName, servicesSectionName, "NNTP_Server", getGlobal("NNTP_Server"));
	SetNameValuePair(fileName, servicesSectionName, "POP_Server", getGlobal("POP_Server"));
	SetNameValuePair(fileName, servicesSectionName, "IMAP_Server", getGlobal("IMAP_Server"));
	//may want this soon
	SetNameValuePair(fileName, servicesSectionName, "Default_Mail_Protocol", getGlobal("Default_Mail_Protocol"));
	


//
//	setLDAPServers(fileName);	//never had this either
//	
	SetNameValuePair(fileName, publishingSectionName, "Publish_URL", getGlobal("Publish_URL"));
	SetNameValuePair(fileName, publishingSectionName, "Publish_Password", getGlobal("Publish_Password"));
	SetNameValuePair(fileName, publishingSectionName, "View_URL", getGlobal("View_URL"));
//
//	Legacy stuff, won't use it no more
//	SetNameValuePair(fileName, netbeuiSectionName, "Enabled", getGlobal("NetBEUIEnabled"));
//
//	SetNameValuePair(fileName, securitySectionName, "SecurityDevice", getGlobal("SecurityDevice"));
//
//	SetNameValuePair(fileName, scriptSectionName, "ScriptEnabled", getGlobal("ScriptEnabled"));
	SetNameValuePair(fileName, scriptSectionName, "ScriptFileName", getGlobal("ScriptFileName"));
	
	//if we decide to support per POP CFG Files, uncomment these lines
	SetNameValuePair(fileName, configurationSectionName, "ConfigurationFileName", getGlobal("ConfigurationFileName"));

	
	//set the flag from ACCTSET.INI
	SetNameValuePair("ACCTSET.INI", "Existing Acct Mode", "RegPodURL", getGlobal("RegPodURL"));
	nciDirty(false);
}



//EVENT HANDLERS

//this is the onLoad event Handler
function loadData()
{
	if (parent.nci && parent.nci.loadData)
	{
		parent.nci.loadData();

		//generate toolbar controls (and load help file if needed)
		if (parent.parent && parent.parent.controls && parent.parent.controls.generateToolBarControls)
			parent.parent.controls.generateToolBarControls();
		
	}
	else
		setTimeout("loadData()", 500);
	//debug("in getdata of nciglob.js");
}

//checks validity of entered data to see if we're ready to unload
function checkData()
{
	if (nciDirty(null))
	{
		if (parent.tabs.tabbody.checkData)
		{
			//alert("checkdata exists");
			var checkResult = parent.tabs.tabbody.checkData();
			if (checkResult != true)
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
	//alert("in saveifdirty");
	var fName = null; 
	
	if (nciDirty(null))
	{
		if (confirm("You have made changes to this configuration.  Would you like to save them?"))
			fName = saveNewOrOldFile();
		nciDirty(false);
	}
	else
		debug("Won't save - NCI not dirty");

	return fName;
}


// checks if fileName exists, if not asks about saving to a new file, 
// else saves over an old file (no prompting)
function saveNewOrOldFile()
{
	//flush data from currenly open tab into globals
	if (parent.tabs && parent.tabs.tabbody && parent.tabs.tabbody.saveData)
	{
		parent.tabs.tabbody.saveData();
	}

	nciDirty(false);

	var fileName = getGlobal("NciFileName");
	if ((fileName == null) || (fileName == "") || (fileName == "_new_"))
	{
		//debug("Saving: must ask for FileName");
		var theName = askNCIFileNameAndSave();
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
	//do a checkdata
	var checkResult = checkData();
	if (checkResult != true)
		return false;
			
	//first make sure any current changes in the open tab are reflected in the globals
	if(parent.tabs.tabbody.saveData)
	{
		parent.tabs.tabbody.saveData();
	}
	
	saveIfDirty();	//save, if need be
	return true;	
}


////*DEFUNCT FUNCTION GRAVEYARD BELOW*////

// Decided not to do LDAPs here, since they can be done in the Config Editor - 
// but let's keep this code around in case we ever do decide
//function setLDAPServers(fileName)
//{
//	var servicesSectionName	=	"Services";
//	var curLdap				=	"";
//	var allLdaps			=	"";
//	var curFlagName			=	"";
//
//	var num_Ldaps = parseInt(GetNameValuePair(fileName, servicesSectionName, "NUM_LDAP_Servers"));
//	if ((!(num_Ldaps)) || (isNaN(num_Ldaps)))
//		num_Ldaps = 0;
//	
//	setGlobal("NUM_LDAP_Servers", num_Ldaps.toString());
//	
//	for (var index = 1; index <= num_Ldaps; index++)	
//	{
//	
//		curFlagName = ("LDAP_Server_"+(index));
//		
//		curLdap = GetNameValuePair(fileName, servicesSectionName, curFlagName);
//		
//		//might need some platform specific newline code in here
//		if ((curLdap != null) && (curLdap != ""))
//		{
//			//curLdap = unescape(curLdap);
//			allLdaps = (allLdaps +  curLdap + "\r");
//		}
//	}
//	
//	setGlobal("LDAP_Servers", allLdaps);
//}

//function setLDAPServers(fileName)
//{
//	var servicesSectionName	=	"Services";
//	var curLdap				=	"";
//	var allLdaps			=	"";
//	var curFlagName			=	"";
//
//	var num_LDAPS = parseInt(getGlobal("NUM_LDAP_Servers"));
//	
//	if ((!num_LDAPS) || (isNaN(num_LDAPS)))
//		num_LDAPS = 0;
//	
//	SetNameValuePair(fileName, servicesSectionName, "NUM_LDAP_Servers", num_LDAPS.toString());
//	
//	//FIXME FIXME - may need platform specific newline handling code here
//	
//	var allLdaps = getGlobal("LDAP_Servers").split("\r");
//	
//	for (var index = 0; index < num_LDAPS; index++)	
//	{
//		curLdap = allLdaps[index];
//		if ((curLdap != null) && (curLDap))
//		{
//			curLdap = escape(curLdap);
//			curFlagName = ("LDAP_Server_"+(index+1));
//			SetNameValuePair(fileName, servicesSectionName, curFlagName, curLdap);
//		}
//	}
//}
//

//returns the filename  that we are working with - from the 
//editfield in the parent's "config" frame
//function getNCINameFromConfigFrame()
//{
	
//	//get the configName from the select list
//	var selectedIndex	= parent.parent.document.NCICONFIGSELECT.NCICONFIGLIST.selectedIndex;
//	//var cConfigName 	= parent.parent.document.NCICONFIGSELECT.NCICONFIGLIST[selectedIndex];
//	var name			= parent.parent.document.NCICONFIGSELECT.NCICONFIGLIST[selectedIndex].text;
//	var cConfigName		= parent.parent.document.NCICONFIGSELECT.NCICONFIGLIST[selectedIndex].value;
//	
//	debug("config name is: " + cConfigName + ", name is: " +  name + ", val: " + cConfigName);
	
//	return cConfigName;
//}

// end hiding contents from old browsers  -->
