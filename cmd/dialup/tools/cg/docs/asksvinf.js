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
	
 	var askSaveFlag 		= 	"";
 	var	savePasswords		= 	"";
 	var asFormName			=	"onlaccoptions";
 
 	modeSectionName			=	"Mode Selection";
 	newSectionName			=	"New Acct Mode";	
 	existingSectionName		=	"Existing Acct Mode";
 	
	
	//set the online/offline flags
	askSaveFlag 		= parent.parent.globals.GetNameValuePair(acctSetupFile,newSectionName,"AskSaveAcctInfo");
	savePasswords		= parent.parent.globals.GetNameValuePair(acctSetupFile,newSectionName,"SavePasswords");
	
	if ((askSaveFlag != null) && (askSaveFlag != "") && (askSaveFlag == "no"))
	{
		setRadio(asFormName, "saveinfo", 2); 
	}
	else if ((savePasswords != null) && (savePasswords != "") && (savePasswords == "yes"))
	{
		setRadio(asFormName, "saveinfo", 0);
	}
	else
	{
		setRadio(asFormName, "saveinfo", 1);
	}
		
	if (parent.controls.generateControls)	parent.controls.generateControls();
}//function loadData()


function checkData()
{
	return true;
}


function saveData()
{
	var acctSetupFile		=	parent.parent.globals.getAcctSetupFilename(parent.parent.globals);
 	var asFormName			=	"onlaccoptions";
 
 	modeSectionName			=	"Mode Selection";
 	newSectionName			=	"New Acct Mode";
 	existingSectionName		=	"Existing Acct Mode";

	var saveRadio = getRadio(asFormName, "saveinfo");
	var saveRadioString = new String(saveRadio);

	if (saveRadioString == "yes")
	{
	 parent.parent.globals.SetNameValuePair(acctSetupFile, newSectionName, "AskSaveAcctInfo", "yes");
	 parent.parent.globals.SetNameValuePair(acctSetupFile, newSectionName, "SavePasswords", "yes");
	}
	else if ((!isNaN(parseInt(saveRadio))) && (saveRadio == 2))
	{
	 parent.parent.globals.SetNameValuePair(acctSetupFile, newSectionName, "AskSaveAcctInfo", "no");
	 parent.parent.globals.SetNameValuePair(acctSetupFile, newSectionName, "SavePasswords", "no");	
	}
	else
	{
		//default - set saveInfo to yes, set savePasswords to no
	 parent.parent.globals.SetNameValuePair(acctSetupFile, newSectionName, "AskSaveAcctInfo", "yes");
	 parent.parent.globals.SetNameValuePair(acctSetupFile, newSectionName, "SavePasswords", "no");	
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
