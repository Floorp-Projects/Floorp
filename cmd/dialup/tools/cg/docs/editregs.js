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

//the file that includes this must also include settings.js!

function loadData()
{
	
	var acctSetupFile		=	parent.parent.globals.getAcctSetupFilename(parent.parent.globals);
	//debug("filename: " + acctSetupFile);
	
 	var tempFlagValue 		= 	"";
 	var asFormName			=	"iasoptions";
 
 	modeSectionName			=	"Mode Selection"
 	newSectionName			=	"New Acct Mode"	
 	existingSectionName		=	"Existing Acct Mode"
 	
	//fill in the lists
	fillIASFileList(asFormName, "SINGLEIAS");
	
	//set the poplist flag
	tempFlagValue = parent.parent.globals.GetNameValuePair(acctSetupFile, newSectionName,"RegServer")
	
	if ((tempFlagValue != null) && (tempFlagValue != ""))
		setRadio(asFormName, "poplist", "yes"); 
	else
		setRadio(asFormName, "poplist", "no");
		
	if (parent.controls.generateControls)	parent.controls.generateControls();
	
}//function loadData()


function checkData()
{
	return true;
}


function saveData()
{
	// save globals here

	var acctSetupFile 		=	parent.parent.globals.getAcctSetupFilename(parent.parent.globals);
	var modeSectionName		= 	"Mode Selection";
	var existingSectionName	=	"Existing Acct Mode";
	var newSectionName		=	"New Acct Mode";
	var	asFormName			=	"iasoptions";

	if (acctSetupFile != null && acctSetupFile != "")	{

		var existingChosen 		=	getRadio(asFormName, "poplist");

		if (existingChosen == "yes")
		{
			 parent.parent.globals.SetNameValuePair(acctSetupFile, newSectionName, "RegServer", getSelectListSelection(asFormName, "SINGLEIAS"));	
		}
		else
		{
			 parent.parent.globals.SetNameValuePair(acctSetupFile, newSectionName, "RegServer", "");	
	
		}
	}  // if (acctSetupFile != null && acctSetupFile != "")	
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
