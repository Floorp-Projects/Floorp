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
	
 	var flagValue 			= 	"";
 	var asFormName			=	"askimapldapoptions";
 
 	//modeSectionName			=	"Mode Selection"
 	//newSectionName			=	"New Acct Mode"	
 	existingSectionName		=	"Existing Acct Mode"
 		
	//set the checkbox flags
	flagValue = yesNoToNoYes( parent.parent.globals.GetNameValuePair(acctSetupFile,existingSectionName,"AskIMAP"));
	setCheckBox(asFormName, "askimap", flagValue);
	
	//got rid of this
	//flagValue = yesNoToNoYes( parent.parent.globals.GetNameValuePair(acctSetupFile,existingSectionName,"AskLDAP"));	
	//setCheckBox(asFormName, "askldap", flagValue);
		
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
	var	asFormName			=	"askimapldapoptions";
	var	flagValue			=	"";

	if (acctSetupFile != null && acctSetupFile != "")	{

			flagValue = yesNoToNoYes(getCheckBox(asFormName, "askimap"));
		 parent.parent.globals.SetNameValuePair(acctSetupFile, existingSectionName, "AskIMAP", flagValue);	

			//no longer applicable
			//flagValue = yesNoToNoYes(getCheckBox(asFormName, "askldap"));
			//parent.parent.globals.SetNameValuePair(acctSetupFile, existingSectionName, "AskLDAP", flagValue);	

	}  // if (acctSetupFile != null && acctSetupFile != "")	
}


 
// end hiding contents from old browsers  -->
