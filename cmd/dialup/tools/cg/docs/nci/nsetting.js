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


//	THIS IS THE API for functions you will want to access from outside of this javascript file:
//	you probably want to use these functions in the OnLoad and OnUnload event handlers of your htmls files,
//	in order 	(1) Get all the appropriate information from the config files and set the appropriate form fields on load
//	and			(2)	To write the data from your form fields back out to the config files on unload	

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	these functions interface with the config files (e.g., ACCTSET.INI)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//	function GetNameValuePair(fileName, sectionName, flagName)			//	looks in the file fileName for a line that looks like "flagName=value"
																		//	under the section called "[sectionName]".  If it finds one, returns value.
																		
//	function SetNameValuePair(fileName, sectionName, flagName, data)	//	in the file fileName, writes a line that looks like "flagname=data" under
																		//	the section called "[sectionName]" -will create the file and/or section name
																		//	if necessary	

//	function getFileListFromConfigFolder(fileSuffix)					//	returns a list of files with the specified suffix that live
																		//	in the Config Folder
																		
																		
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	these functions set the elements of your form	(all return nothing)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	function setCheckBox(formName, boxName, inValue)	//	if inValue = 0 sets checkbox to off, else to on
//	function setRadio(formName, radioName, inValue)		//	inValue is yes or no - yes is first, no is second radio.  If inValue is a number, sets
														//	the inValueth radio with that name to on.  (0 < inValue < 33, jsut for sanity checking0
//	function setText(formName, textFieldName, inValue)	//	sets the specified textField to the input value

//	function setCardTypes(formName, cardTypesString)		// cardTypesString is a string of the form "AX,MC,DC,VI" as per ACCTSET.INI requirements, this sets
												// four checkboxes whose names MUST be: "AX", "MC", "DC", and "VI"
//	function fillSelectListWithConfigFiles(formName, selectListName, fileSuffix, offerNew)	// finds all files with the suffix fileSuffix
																					// (should look like ".NCI") and fills in the specified list
																					// this is used by the following two functions
																					// if offerNew is true, an option will be provided with name
																					// "New Configuration" and value "_new_"

//	function fillNCIFileList(formName, selectListName)	//will fill the specified select list with the names of all NCI files in the Config folder
//	function fillIASFileList(formName, selectListName)	//will fill the specified select list with the names of all IAS files in the Config folder
														//selects RegServ.SR if that file exists
//	function setSelectList(formName, selectListName, inValue)	// sets the selectlist to select the option whose text is inValue, if it exists
																// used internally to set the value of the NCI file list, for example
																
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	these functions get the values of elements of your form and return text suitable for writing into Config files (all return strings)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	function getRadio(formName, radioName)			// 	returns "yes" if first radio is selected, "no" if second is selected, else returns
													//	index of selected radio (-1 if none selected)
//	function getText(formName, textFieldName)		//	returns the text inside a text field
//	function getCheckBox(formName, selectListName)	//	returns true if checkbox is checked, false if not
//	function getSelectListSelection(formName, selectListName)

////////these are specific functions which get specfic information using the above functions//////////////////
//	function getCardTypes(formName)		// returns a string of the form "AX,MC,DC,VI", based on the values of the four checkboxes in
										// the specified form, whose names MUST be: "AX", "MC", "DC", and "VI"
										
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Other Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//function yesNoToNoYes(inValue)	// if inValue == "yes" returns "no".  if invalue == "no" returns yes, else nothing

//function getAcctSetupFilename()	//returns the name of ACCTSET.INI

//function debug(theString)	// writes the string to the java console, using the setUpPlugin

//these should be overriden if this file is included from within another javascript file
//	function checkData()		// this should return true in most cases, you can add checks in here for determining if any necessary fields
								// have been left empty, etc. - if it returns false, account setup's global navigation won't proceed when next is clicked
//	function getAcctSetFlags()	// this is an example of a function that would be used as an onLoad handler in a file where the form name is
								// "acctsetForm", and it has form elements for all the flags in ACCTSET.INI 
//	function saveAcctSetFlags()	// this is an example of a function that would be used as an onUnLoad handler in a file where the form name is
								// "acctsetForm", and it has form elements for all the flags in ACCTSET.INI 

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	LOCATION DEPENDENT FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following functions are dependent on the LOCATION OF THIS FILE and the LOCATION OF THE globals.js FILE
//	actually, they are really dependent on the part of the frameset heirarchy in which this file is included
//	right now we assume it is included two framesets deep, and the outermost frameset contains a frame called "globals", 
//	which includes the "globals.js" file which is the meat of the account setup javascript universe - it interfaces with the plugin
//  SO, if the heirarchy changes, the path of the "globals" frame should be updated

//	function getGlobalsLocation()


//this is the function that determines where access to the globals is
//from the document that is including this file.
//it's made out of ugly hacks and ricotta cheese
function getGlobalsLocation()
{
	var loc = ""; 

	//this stuff is no longer necessary, since we now do everything in the same window,
	//and the globals are always in the same relative location.	
	//if (top.name == "addnci")
	//{	
	//	//alert("addnci!");
	//	loc = ("top.opener.top.globals");
	//}
	
	//else if (top.name == "addias")
	//{
	//	loc = ("top.window.opener.globals");
	//}
	//else
	//{
		//standard case, globals is in the same window
		var loc = "top.globals";
		
	//}
	return loc;
}



function getAcctSetupFilename()
{
	return 	"ACCTSET.INI";
}

function completeConfigFilePath(inFileName)
{
	var outFileName = top.globals.getConfigFolder(top.globals) + inFileName;
	return outFileName;
}


///WARNING THE LOCATION OF THE GLOBALS IS HARDCODED IN THIS FCN - it can only be accessed by children windows
function saveNewConfigFile(fileName, inValue, doPrompt)
{
	// Request privilege
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if ((doPrompt != true) && (doPrompt != false))
		doPrompt = false;	//should we really assume false?
	
	if ((fileName != null) && (fileName != ""))
	{
		var cFileName = completeConfigFilePath(fileName);
		
		if ((cFileName != null) && (inValue != null) && (cFileName != "") && (inValue != ""))
			top.globals.document.setupPlugin.SaveTextToFile(cFileName, inValue, doPrompt);
		
	}
}

function debug(theString)
{
	top.globals.debug(theString);
}


function GetNameValuePair(inFileName, sectionName, flagName)
{
	// Request privilege
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var fileName = completeConfigFilePath(inFileName);
	
	var data = top.globals.document.setupPlugin.GetNameValuePair(fileName, sectionName, flagName);
	
	
	debug("\tGetNameValuePair: (" + inFileName + ") [" +sectionName+ "] " +flagName+ "=" + data);
	return data;
}

function SetNameValuePair(inFileName, section, variable, data)
{
	// Request privilege
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var fileName = completeConfigFilePath(inFileName);

	top.globals.document.setupPlugin.SetNameValuePair(fileName, section, variable, data);
	debug("\tSetNameValuePair: (" + fileName + ")[" +section+ "] " +variable+ "=" + data);
}

function getFileListFromConfigFolder(fileSuffix)
{
	// Request privilege
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var pathName = top.globals.getConfigFolder(top.globals);
	var theList = top.globals.document.setupPlugin.GetFolderContents(pathName, fileSuffix);
	return theList;
}

function fillSelectListWithConfigFiles(formName, selectListName, fileSuffix, offerNew)
{
	var curConfigName = "";
	var curConfigDescription = "";

	//blank out old list
	for (var i = (parent.tabs.tabbody.document[formName][selectListName].length -1); i >= 0 ; i--)
	{
		parent.tabs.tabbody.document[formName][selectListName].options[i] = null;
	}	
	
	//make a blank so the user can choose nothing
	parent.tabs.tabbody.document[formName][selectListName].options[0] = new Option("< None Selected >","",false, false); 
	
	//Only offer a new if we said so
	if (offerNew == true)
		parent.tabs.tabbody.document[formName][selectListName].options[1] = new Option("< New Configuration >","_new_",false, false); 

	// Get a list of the files in the Config folder
	fileList = getFileListFromConfigFolder(fileSuffix);
	
	//debug( "filelist: " + fileList);
	
	if (fileList != null)	{
		for (var i=0; i<fileList.length; i++)	{
				curConfigName 			= GetNameValuePair(fileList[i], "Dial-In Configuration", "SiteName");
				curConfigDescription 	= GetNameValuePair(fileList[i], "Dial-In Configuration", "Description");

				if ((curConfigName != null)  && (curConfigName != ""))
					parent.tabs.tabbody.document[formName][selectListName].options[parent.tabs.tabbody.document[formName][selectListName].length] = new Option(curConfigName,fileList[i], false, false);
				else if ((curConfigDescription != null) && (curConfigDescription != ""))
					parent.tabs.tabbody.document[formName][selectListName].options[parent.tabs.tabbody.document[formName][selectListName].length] = new Option(curConfigDescription,fileList[i], false, false);
				else
					parent.tabs.tabbody.document[formName][selectListName].options[parent.tabs.tabbody.document[formName][selectListName].length] = new Option(fileList[i],fileList[i], false, false);
					
			}
		}
}


//this is a helper function that converts "yes" to 0 and "no" to 1 - but leaves integers alone
function ynToZeroOne(inValue)
{
	var ynValue = new String(inValue);
	var intValue = -1;
	
	if (ynValue == "yes") 
	{
		intValue = 0;	
	}
	
	else if (ynValue == "no")
	{
		intValue = 1;		
	}
	
	else
	{
	if (!isNaN(parseInt(inValue)))
		{
				intValue = inValue;
		}
	}
	return intValue;	
}


function yesNoToNoYes(inValue)
{
	var tempValue = new String(inValue);
	
	if ((tempValue != null))
	{
		if (tempValue == "yes")
		{
			return "no";	
		}		
		else if (tempValue == "no")
		{
			return "yes";
		}	
		else
			return inValue;	
	}
}

//set checkbox on if inValue is "yes" or 0, off otherwise
function setCheckBox(formName, boxName, inValue)
{

	debug("SetCheckbox: " + inValue);
	var intValue = ynToZeroOne(inValue);

	if (intValue == 0)
		parent.tabs.tabbody.document[formName][boxName].checked = 1;
	else
		parent.tabs.tabbody.document[formName][boxName].checked = 0;
}

function getCheckBox(formName, boxName)		// returns yes if checked, no if not
{
	var outValue = "no";
	
	var tfValue =	(parent.tabs.tabbody.document[formName][boxName].checked);
		
	if (tfValue == true)
		outValue = "yes";
		
	return outValue;	
}

function setRadio(formName, radioName, inValue)
{
	var intValue = ynToZeroOne(inValue);

	parent.tabs.tabbody.document[formName][radioName][intValue].checked = 1;
	debug("Setting radio " +radioName + " to " + intValue);
}

function getRadio(formName, radioName)
{
	
	var radioIndex = -1;
	//based on the assumption that yes is the first radio, "no" is the second radio
	
	if (parent.tabs.tabbody.document[formName][radioName][0].checked == 1)
	{
		//debug("getRadio: returning yes");
		return ("yes");
	}
	else if (parent.tabs.tabbody.document[formName][radioName][1].checked == 1)
	{	
		//debug("getRadio: returning no");
		return ("no");
	}
	else
	{
		//debug("getRadio: radio[0]" + parent.tabs.tabbody.document[formName][radioName][0].checked);
		//debug("getRadio: radio[1]" + parent.tabs.tabbody.document[formName][radioName][1].checked);
		 
		for (var i = 0; i < parent.tabs.tabbody.document[formName][radioName].length; i++)
		{
			if (parent.tabs.tabbody.document[formName][radioName][i].checked == 1)	
				radioIndex = i;		
		}
		return radioIndex;
	}
}

function getText(formName, textFieldName)
{
	//return document[formName][textFieldName].value;
	return parent.tabs.tabbody.document[formName][textFieldName].value;
}

function setText(formName, textFieldName, inValue)
{
	debug("Setting Text for " + textFieldName + " to " + inValue);
	//document[formName][textFieldName].value = inValue;
	parent.tabs.tabbody.document[formName][textFieldName].value = inValue;
}


function setSelectList(formName, selectListName, inValue)
{
	if (inValue != null)
	{ 
	 	for (var i = (parent.tabs.tabbody.document[formName][selectListName].length - 1); i >= 0 ; i--)
	 	{
	 		if(inValue.toString() == (parent.tabs.tabbody.document[formName][selectListName].options[i].value.toString()))
	 		{
	 			//debug("Found select list Match (" + i + ")(" + inValue + ")");
	 			parent.tabs.tabbody.document[formName][selectListName].options[i].selected = true;
	 		}
	 	}
	}	
}



function fillNCIFileList(formName, listName)
{
		fillSelectListWithConfigFiles(formName, listName, ".NCI", true);
		
		var acctSetupFile = getAcctSetupFilename();
 		var selectedNCIFile = GetNameValuePair(acctSetupFile,"Mode Selection","ExistingSRFile");
		setSelectList(formName, listName, selectedNCIFile);
} // fillNCIFileList



function getSelectListSelection(formName, selectListName)
{
	return	parent.tabs.tabbody.document[formName][selectListName].options[parent.tabs.tabbody.document[formName][selectListName].options.selectedIndex].value.toString();
}


// this is a smaple onLoad event handler - this would be used if the form in question
// had an element for each and every flag in ACCTSET.INI
function getAcctSetFlags()
{
	

	var acctSetupFile		=	getAcctSetupFilename();
 	var tempFlagValue 		= 	"";
 	var asFormName			=	"acctsetForm";
 	
 	//parent.parent.parent.status = ("loading data from " + acctSetupFile + "...");
 	
 	//get all the flags related to mode selection, set the appropriate form fields (4)
	tempFlagValue = GetNameValuePair(acctSetupFile,"Mode Selection","ForceExisting");
	setRadio(asFormName, "ForceExisting", tempFlagValue);
		
	fillNCIFileList(asFormName, "ExistingSRFile");
	
	tempFlagValue = GetNameValuePair(acctSetupFile,"Mode Selection","ForceNew");
	setRadio(asFormName, "ForceNew", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Mode Selection","IntlMode");
	setRadio(asFormName, "IntlMode", tempFlagValue);
	
	//get all the flags related to new account mode, set the appropriate form fields (9)
	tempFlagValue = GetNameValuePair(acctSetupFile,"New Acct Mode","ShowNewPathInfo");
	setRadio(asFormName, "ShowNewPathInfo", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"New Acct Mode","AskSaveAcctInfo");
	setRadio(asFormName, "AskSaveAcctInfo", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"New Acct Mode","SavePasswords");
	setRadio(asFormName, "SavePasswords", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"New Acct Mode","AskPersonalInfo");
	setRadio(asFormName, "AskPersonalInfo", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"New Acct Mode","AskBillingInfo");
	setRadio(asFormName, "AskBillingInfo", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"New Acct Mode","RegServer");
	setText(asFormName, "RegServer", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"New Acct Mode","CardTypes");
	setCardTypes(asFormName, tempFlagValue);
	
	//get all the flags related to Existing account mode, set the appropriate form fields (15)
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","ShowExistingPathInfo");
	setRadio(asFormName, "ShowExistingPathInfo", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","RegPodURL");
	setText(asFormName, "RegPodURL", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","ShowIntro2");
	setRadio(asFormName, "ShowIntro2", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","ShowPhones");
	setRadio(asFormName, "ShowPhones", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskName");
	setRadio(asFormName, "AskName", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskLogin");
	setRadio(asFormName, "AskLogin", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskTTY");
	setRadio(asFormName, "AskTTY", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskEmail");
	setRadio(asFormName, "AskEmail", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskPhone");
	setRadio(asFormName, "AskPhone", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskDNS");
	setRadio(asFormName, "AskDNS", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskHosts");
	setRadio(asFormName, "AskHosts", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskPublishing");
	setRadio(asFormName, "AskPublishing", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskRegister");
	setRadio(asFormName, "AskRegister", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskLDAP");
	setRadio(asFormName, "AskLDAP", tempFlagValue);
	tempFlagValue = GetNameValuePair(acctSetupFile,"Existing Acct Mode","AskIMAP");
	setRadio(asFormName, "AskIMAP", tempFlagValue);
	
	//parent.parent.status = ("Loading Data.....Done.");
}//function getAcctSetFlags()


function checkData()
{
	return true;
}


//sample onUnload event handler - this would be used if the form in question
// had an element for each and every flag in ACCTSET.INI
function saveAcctSetFlags()
{
	// save globals here

	var modeSectionName		= 	"Mode Selection";
	var existingSectionName	=	"Existing Acct Mode";
	var newSectionName		=	"New Acct Mode";
	var	asFormName			=	"acctsetForm";

	//debug("trying to save...");

	// Request privilege
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (top.globals.document.setupPlugin == null)	return;

	var acctSetupFile = getAcctSetupFilename();
	
	debug("writing data to " + acctSetupFile);
	
	if (acctSetupFile != null && acctSetupFile != "")	{

		SetNameValuePair(acctSetupFile,modeSectionName,"ForceExisting", getRadio("acctsetForm", "ForceExisting"));
		SetNameValuePair(acctSetupFile,modeSectionName,"ExistingSRFile", getSelectListSelection("acctsetForm", "ExistingSRFile"));
		SetNameValuePair(acctSetupFile,modeSectionName,"ForceNew", getRadio(asFormName,"ForceNew"));
		SetNameValuePair(acctSetupFile,modeSectionName,"IntlMode", getRadio(asFormName,"IntlMode"));
		
		
		SetNameValuePair(acctSetupFile,newSectionName,"ShowNewPathInfo", getRadio(asFormName,"ShowNewPathInfo"));
		SetNameValuePair(acctSetupFile,newSectionName,"AskSaveAcctInfo", getRadio(asFormName,"AskSaveAcctInfo"));
		SetNameValuePair(acctSetupFile,newSectionName,"SavePasswords", getRadio(asFormName,"SavePasswords"));
		SetNameValuePair(acctSetupFile,newSectionName,"AskPersonalInfo", getRadio(asFormName,"AskPersonalInfo"));
		SetNameValuePair(acctSetupFile,newSectionName,"AskBillingInfo", getRadio(asFormName,"AskBillingInfo"));
		SetNameValuePair(acctSetupFile,newSectionName,"RegServer", getText(asFormName, "RegServer"));

		SetNameValuePair(acctSetupFile,newSectionName,"CardTypes", getCardTypes(asFormName));
		
		
		SetNameValuePair(acctSetupFile,existingSectionName,"ShowExistingPathInfo", getRadio(asFormName,"ShowExistingPathInfo"));
		SetNameValuePair(acctSetupFile,existingSectionName,"RegPodURL", getText(asFormName, "RegPodURL"));
		SetNameValuePair(acctSetupFile,existingSectionName,"ShowIntro2", getRadio(asFormName,"ShowIntro2"));
		SetNameValuePair(acctSetupFile,existingSectionName,"ShowPhones", getRadio(asFormName,"ShowPhones"));
		SetNameValuePair(acctSetupFile,existingSectionName,"AskName", getRadio(asFormName,"AskName"));
		SetNameValuePair(acctSetupFile,existingSectionName,"AskLogin", getRadio(asFormName,"AskLogin"));

		SetNameValuePair(acctSetupFile,existingSectionName,"AskTTY", getRadio(asFormName,"AskTTY"));
		SetNameValuePair(acctSetupFile,existingSectionName,"AskEmail", getRadio(asFormName,"AskEmail"));
		SetNameValuePair(acctSetupFile,existingSectionName,"AskPhone", getRadio(asFormName,"AskPhone"));

		SetNameValuePair(acctSetupFile,existingSectionName,"AskDNS", getRadio(asFormName,"AskDNS"));
		SetNameValuePair(acctSetupFile,existingSectionName,"AskHosts", getRadio(asFormName,"AskHosts"));
		SetNameValuePair(acctSetupFile,existingSectionName,"AskPublishing", getRadio(asFormName,"AskPublishing"));
		SetNameValuePair(acctSetupFile,existingSectionName,"AskRegister", getRadio(asFormName,"AskRegister"));
		SetNameValuePair(acctSetupFile,existingSectionName,"AskLDAP", getRadio(asFormName,"AskLDAP"));

		SetNameValuePair(acctSetupFile,existingSectionName,"AskIMAP", getRadio(asFormName,"AskIMAP"));
		}
}


 //	For easy reference, this is the list of flags that ACCTSET.INI currently supports
 //	ForceExisting
 // ExistingSRFile
 //	ForceNew
 //	IntlMode
 //	ShowNewPathInfo
 //	AskSaveAcctInfo
 //	SavePasswords
 //	AskPersonalInfo
 //	//AskSurvey		//defunct
 //	//AskShareInfo	//defunct
 //	AskBillingInfo
 //	RegServer
 //	CardTypes
 //	ShowExistingPathInfo
 //	RegPodURL
 //	ShowIntro2
 // ShowPhones
 //	AskName
 //	AskLogin
 //	AskTTY
 //	AskEmail
 //	AskPhone
 //	AskDNS
 //	AskHosts
 // AskPublishing
 //	AskRegister
 //	AskLDAP
 //	AskIMAP



// end hiding contents from old browsers  -->
