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


// Request privilege
var thePlatform = new String(navigator.userAgent);
var x=thePlatform.indexOf("(")+1;
var y=thePlatform.indexOf(";",x+1);
thePlatform=thePlatform.substring(x,y);

if (thePlatform != "Macintosh")	{
	compromisePrincipals();			// work around for the security check
	}

function loadData()
{

	
	var acctSetupFile		= 	parent.parent.globals.getAcctSetupFilename(parent.parent.globals);
 	var forceExisting 		= 	"";
 	var	forceNew			= 	"";
 	var showPhones			=	"";
 	var asFormName			=	"asglobals";
 
 	modeSectionName			=	"Mode Selection";
 	newSectionName			=	"New Acct Mode";	
 	existingSectionName		=	"Existing Acct Mode";
 	
	
	//set the online/offline flags
	forceExisting 	= parent.parent.globals.GetNameValuePair(acctSetupFile,modeSectionName,"ForceExisting");
	forceNew		= parent.parent.globals.GetNameValuePair(acctSetupFile,modeSectionName,"ForceNew");
	
	if ((forceNew != null) && (forceNew != "") && (forceNew == "yes"))
	{
		setRadio(asFormName, "asmode", 0); 
	}
	else if ((forceExisting != null) && (forceExisting != "") && (forceExisting == "yes"))
	{
		setRadio(asFormName, "asmode", 1);
	}
	else
	{
		setRadio(asFormName, "asmode", 2);
	}
	//set the intlMode flag
	setCheckBox(asFormName, "intmode", parent.parent.globals.GetNameValuePair(acctSetupFile,modeSectionName,"IntlMode"));
	
	//set the two other options flags
	setCheckBox(asFormName, "Show_Intro_Screens", parent.parent.globals.GetNameValuePair(acctSetupFile,modeSectionName,"Show_Intro_Screens"));	
	var Dialer_Idle_Time = parseInt(parent.parent.globals.GetNameValuePair(acctSetupFile,modeSectionName,"Dialer_Disconnect_After"));
	
	if (Dialer_Idle_Time == null || isNaN(Dialer_Idle_Time) || Dialer_Idle_Time == "null" || Dialer_Idle_Time == "" || Dialer_Idle_Time < 5)
		Dialer_Idle_Time = 15; 	//default idle time
	
	setText(asFormName, "Dialer_Disconnect_After", Dialer_Idle_Time);
	
	if (parent.controls.generateControls)	
		parent.controls.generateControls();
	
}//function loadData()


function fixIdleTime()
{
 	var asFormName			=	"asglobals";
	
	var Dialer_Idle_Time = parseInt(getText(asFormName,"Dialer_Disconnect_After"));
	
	if (Dialer_Idle_Time.toString() == "NaN" || Dialer_Idle_Time > 99)
	{
		Dialer_Idle_Time = 15; // default dialer idle time
	}
	else if (Dialer_Idle_Time < 5)
		Dialer_Idle_Time = 5; //minimum dialer idle time

	setText(asFormName, "Dialer_Disconnect_After", Dialer_Idle_Time);
}

function checkData()
{
	fixIdleTime();
	return true;
}


function saveData()
{
	var acctSetupFile		=	parent.parent.globals.getAcctSetupFilename(parent.parent.globals);
 	var asFormName			=	"asglobals";
 
 	modeSectionName			=	"Mode Selection";
 	newSectionName			=	"New Acct Mode";
 	existingSectionName		=	"Existing Acct Mode";

	parent.parent.globals.SetNameValuePair(acctSetupFile, modeSectionName,"IntlMode", getCheckBox(asFormName,"intmode"));

	var modeRadio = getRadio(asFormName, "asmode");
	var modeRadioString = new String(modeRadio);
	if (modeRadioString == "yes")
	{
		//debug("SaveData: Ext no, New yes");
		parent.parent.globals.SetNameValuePair(acctSetupFile, modeSectionName, "ForceNew", "yes");
		parent.parent.globals.SetNameValuePair(acctSetupFile, modeSectionName, "ForceExisting", "no");
	}
	else if (modeRadioString == "no")
	{
		//debug("SaveData: New no, Ext yes");
		parent.parent.globals.SetNameValuePair(acctSetupFile, modeSectionName, "ForceNew", "no");
		parent.parent.globals.SetNameValuePair(acctSetupFile, modeSectionName, "ForceExisting", "yes");	
	}
	else
	{
		//debug("SaveData: both no");
		parent.parent.globals.SetNameValuePair(acctSetupFile, modeSectionName, "ForceNew", "no");
		parent.parent.globals.SetNameValuePair(acctSetupFile, modeSectionName, "ForceExisting", "no");	
	}
	
	//set dialer idle time and show_intro screens
	parent.parent.globals.SetNameValuePair(acctSetupFile, modeSectionName, "Show_Intro_Screens", getCheckBox(asFormName, "Show_Intro_Screens"));
	parent.parent.globals.SetNameValuePair(acctSetupFile, modeSectionName, "Dialer_Disconnect_After", getText(asFormName, "Dialer_Disconnect_After"));
	

}

// end hiding contents from old browsers  -->
