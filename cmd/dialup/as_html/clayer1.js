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



compromisePrincipals();


function page( file, section, variable )
{
	this.type = "INFO";
	this.file = file;
	this.section = section;
	this.variable = variable;

	this.fullhistory = false;
	this.top = false;
}

// the following three functions make sense if you go look at the "Next" clause in the function
// "go" below

// ¥ a "condition" is read from the Account Setup init file (typically "Config\ACCTSET.INI"),
// from the section "[section]" and the given variable name, and if the given "value" matches,
// the "file" gets loaded
function condition( file, section, variable, value )
{
	this.type = "CONDITION";
	this.file = file;
	this.section = section;
	this.variable = variable;
	this.value = value;
}

// ¥ a "method" is given in "method" (a JavaScript method), and if when evaluated, it's value
// is "value", the given "file" gets loaded
function method( file, method, value )
{
	this.type = "METHOD";
	this.file = file;
	this.method = method;
	this.value = value;
}

// ¥ an "action" simply loads the given "file"
function action( file )
{
	this.type = "ACTION";
	this.file = file;
}


function checkEditMode()
{
	if ( parent.parent && parent.parent.globals && parent.parent.globals.document.vars.editMode.value.toLowerCase() == "yes" )
	{
		return true;
	}
	else
	{
		return false;
	}
}



pages=new Array();

pages[0]=new Array();
pages[0][0]=new page("main.htm",null,null);
pages[0][1]=new method("ipreview/inpvw1.htm","parent.content.go('Internet Preview')",true);
pages[0][2]=new method("preview/duepvw1.htm","parent.content.go('Preview')",true);
pages[0][3]=new method("intro/intro1.htm","parent.controls.checkShowIntroFlag('')",true);
pages[0][4]=new condition("intro/intro1.htm","Mode Selection","Show_Intro_Screens","yes");
pages[0][5]=new condition("needs1.htm","Mode Selection","ForceNew","yes");
pages[0][6]=new condition("useAcct.htm","Mode Selection","ForceExisting","yes");
pages[0][7]=new action("accounts.htm");

pages[1]=new Array();
pages[1][0]=new page("accounts.htm",null,null);
pages[1][1]=new method("needs1.htm","parent.content.go('New Path')",true);
pages[1][2]=new method("useAcct.htm","parent.content.go('Existing Path')",true);

// New Account Path

pages[ 2 ] = new Array();
pages[ 2 ][ 0 ] = new page( "needs1.htm", "New Acct Mode", "ShowNewPathInfo" );
pages[ 2 ][ 1 ] = new condition( "newAcct.htm", "New Acct Mode", "ShowNewPathInfo", "no" );
pages[ 2 ][ 2 ] = new method( "newAcct.htm", "parent.content.go('')", true );

pages[ 3 ] = new Array();
pages[ 3 ][ 0 ] = new page( "newAcct.htm", "New Acct Mode", "AskPersonalInfo" );
pages[ 3 ][ 1 ] = new condition( "billing.htm", "New Acct Mode", "AskPersonalInfo", "no" );
pages[ 3 ][ 2 ] = new method( "billing.htm", "parent.content.go('')", true );

pages[ 4 ] = new Array();
pages[ 4 ][ 0 ] = new page( "billing.htm", "New Acct Mode", "AskBillingInfo" );
pages[ 4 ][ 1 ] = new condition( "modem1.htm", "New Acct Mode", "AskBillingInfo", "no" );
pages[ 4 ][ 2 ] = new method( "modem1.htm", "parent.content.go('')", true );

pages[ 5 ] = new Array();
pages[ 5 ][ 0 ] = new page( "modem1.htm", null, null );
pages[ 5 ][ 1 ] = new method( "dialinf1.htm", "parent.content.go('')", true );

pages[ 6 ] = new Array();
pages[ 6 ][ 0 ] = new page( "dialinf1.htm", null, null );
pages[ 6 ][ 1 ] = new method( "dialinf2.htm", "parent.content.go('')", true );

pages[ 7 ] = new Array();
pages[ 7 ][ 0 ] = new page( "dialinf2.htm", null, null );
pages[ 7 ][ 1 ] = new method( "download.htm", "parent.content.go('New Path')", true );
pages[ 7 ][ 2 ] = new method( "connect2.htm", "parent.content.go('Existing Path')", true );

pages[ 53 ] = new Array();
pages[ 53 ][ 0 ] = new page( "download.htm", null, null );
pages[ 53 ][ 1 ] = new method( "1step.htm", "parent.content.go( '' )", true );

pages[ 54 ] = new Array();
pages[ 54 ][ 0 ] = new page( "1step.htm", null, null );

pages[ 8 ] = new Array();
pages[ 8 ][ 0 ] = new page( "connect1.htm", null, null );
pages[ 8 ][ 1 ] = new method( "editfour.htm", "checkEditMode('')", true );
pages[ 8 ][ 2 ] = new method( "register.htm", "parent.content.go('')", true );

pages[ 9 ] = new Array();
pages[ 9 ][ 0 ] = new page( "register.htm", null, null );

// Existing Account Path

pages[10]=new Array();
pages[10][0]=new page("useAcct.htm",null,null);
pages[10][1]=new condition("needs2.htm","Mode Selection","ExistingSRFile","...");
pages[10][2]=new method("needs2.htm","parent.content.go('')",true);

pages[11]=new Array();
pages[11][0]=new page("needs2.htm","Existing Acct Mode","ShowExistingPathInfo");
pages[11][1]=new condition("acctInfo.htm","Existing Acct Mode","ShowExistingPathInfo","no");
pages[11][2]=new method("acctInfo.htm","parent.content.go('')",true);

pages[12]=new Array();
pages[12][0]=new page("acctInfo.htm","Existing Acct Mode","AskName");
pages[12][1]=new condition("dial.htm","Existing Acct Mode","AskName","no");
pages[12][2]=new method("dial.htm","parent.content.go('')",true);

pages[13]=new Array();
pages[13][0]=new page("dial.htm","Existing Acct Mode","AskPhone");
pages[13][1]=new condition("namepw.htm","Existing Acct Mode","AskPhone","no");
pages[13][2]=new method("namepw.htm","parent.content.go('')",true);

pages[14]=new Array();
pages[14][0]=new page("namepw.htm","Existing Acct Mode","AskLogin");
pages[14][1]=new condition("email.htm","Existing Acct Mode","AskLogin","no");
pages[14][2]=new method("email.htm","parent.content.go('')",true);

pages[15]=new Array();
pages[15][0]=new page("email.htm","Existing Acct Mode","AskEmail");
pages[15][1]=new condition("servers.htm","Existing Acct Mode","AskEmail","no");
pages[15][2]=new method("servers.htm","parent.content.go('')",true);

pages[16]=new Array();
pages[16][0]=new page("servers.htm","Existing Acct Mode","AskHosts");
pages[16][1]=new condition("dns.htm","Existing Acct Mode","AskHosts","no");
pages[16][2]=new method("dns.htm","parent.content.go('')",true);

pages[17]=new Array();
pages[17][0]=new page("dns.htm","Existing Acct Mode","AskDNS");
pages[17][1]=new condition("publish.htm","Existing Acct Mode","AskDNS","no");
pages[17][2]=new method("publish.htm","parent.content.go('')",true);

pages[18]=new Array();
pages[18][0]=new page("publish.htm","Existing Acct Mode","AskPublishing");
pages[18][1]=new condition("modem1.htm","Existing Acct Mode","AskPublishing","no");
pages[18][2]=new method("modem1.htm","parent.content.go('')",true);

pages[19]=new Array();
pages[19][0]=new page("connect2.htm",null,null);
pages[19][1]=new method(null,"parent.content.go('Connect Now')",true);
pages[19][2]=new method("error.htm","parent.content.go('error.htm')",true);

// Final Screens

pages[20]=new Array();
pages[20][0]=new page("ok.htm",null,null);

pages[21]=new Array();
pages[21][0]=new page("okreboot.htm",null,null);

pages[22]=new Array();
pages[22][0]=new page("error.htm",null,null);
pages[22][1]=new method("register.htm","parent.content.go('New Path')",true);
pages[22][2]=new method("connect2.htm","parent.content.go('Existing Path')",true);

pages[23]=new Array();
pages[23][0]=new page("later.htm",null,null);
pages[23][1]=new method("later.htm","parent.content.go('Done')",true);

// Settings

pages[24]=new Array();
pages[24][0]=new page("settings.htm",null,null);

// About Box

pages[25]=new Array();
pages[25][0]=new page("aboutbox.htm",null,null);

// Manage IAS Servers

pages[26]=new Array();
pages[26][0]=new page("addias.htm",null,null);

// Manage NCI Servers

pages[27]=new Array();
pages[27][0]=new page("addnci.htm",null,null);

// Dialup Edition Preview Screens

pages[28]=new Array();
pages[28][0]=new page("preview/duepvw1.htm",null,null);
//pages[28][0].fullhistory=true;
pages[28][1]=new method("preview/duepvw2.htm","parent.content.go('')",true);

pages[29]=new Array();
pages[29][0]=new page("preview/duepvw2.htm",null,null);
pages[29][1]=new method("preview/duepvw3.htm","parent.content.go('')",true);

pages[30]=new Array();
pages[30][0]=new page("preview/duepvw3.htm",null,null);
pages[30][1]=new method("preview/duepvw4.htm","parent.content.go('')",true);

pages[31]=new Array();
pages[31][0]=new page("preview/duepvw4.htm",null,null);
pages[31][1]=new method("preview/duepvw5.htm","parent.content.go('')",true);

pages[32]=new Array();
pages[32][0]=new page("preview/duepvw5.htm",null,null);
pages[32][1]=new method("preview/duepvw6.htm","parent.content.go('')",true);

pages[33]=new Array();
pages[33][0]=new page("preview/duepvw6.htm",null,null);
pages[33][1]=new method("preview/duepvw7.htm","parent.content.go('')",true);

pages[34]=new Array();
pages[34][0]=new page("preview/duepvw7.htm",null,null);
pages[34][1]=new method("preview/duepvw7a.htm","parent.content.go('')",true);

pages[35]=new Array();
pages[35][0]=new page("preview/duepvw7a.htm",null,null);
pages[35][1]=new method("preview/duepvw8.htm","parent.content.go('')",true);

pages[36]=new Array();
pages[36][0]=new page("preview/duepvw8.htm",null,null);
//pages[36][1]=new method("main.htm","parent.content.go('')",true);
pages[36][1]=new action("main.htm");

// Internet Preview Screens

pages[37]=new Array();
pages[37][0]=new page("ipreview/inpvw1.htm",null,null);
//pages[37][0].fullhistory=true;
pages[37][1]=new method("ipreview/inpvw2.htm","parent.content.go('')",true);

pages[38]=new Array();
pages[38][0]=new page("ipreview/inpvw2.htm",null,null);
pages[38][1]=new method("ipreview/inpvw3.htm","parent.content.go('')",true);

pages[39]=new Array();
pages[39][0]=new page("ipreview/inpvw3.htm",null,null);
pages[39][1]=new method("ipreview/inpvw4.htm","parent.content.go('')",true);

pages[40]=new Array();
pages[40][0]=new page("ipreview/inpvw4.htm",null,null);
pages[40][1]=new method("ipreview/inpvw5.htm","parent.content.go('')",true);

pages[41]=new Array();
pages[41][0]=new page("ipreview/inpvw5.htm",null,null);
pages[41][1]=new method("ipreview/inpvw6.htm","parent.content.go('')",true);

pages[42]=new Array();
pages[42][0]=new page("ipreview/inpvw6.htm",null,null);
pages[42][1]=new method("ipreview/inpvw7.htm","parent.content.go('')",true);

pages[43]=new Array();
pages[43][0]=new page("ipreview/ipreview/inpvw7.htm",null,null);
//pages[43][1]=new method("main.htm","parent.content.go('')",true);	
pages[43][1]=new action("main.htm");									// XXX For Deluxe, change "main.htm" to "ipreview/inpvw8.htm"

pages[44]=new Array();
pages[44][0]=new page("ipreview/inpvw8.htm",null,null);
//pages[44][1]=new method("main.htm","parent.content.go('')",true);
pages[44][1]=new action("main.htm");

// start screen

pages[45]=new Array();
pages[45][0]=new page("start.htm",null,null);
//pages[45][0].top=true;

// intro screens

pages[46]=new Array();
pages[46][0]=new page("intro/intro1.htm",null,null);
//pages[46][0].fullhistory=true;
pages[46][1]=new method("intro/intro2.htm","parent.content.go('')",true);

pages[47]=new Array();
pages[47][0]=new page("intro/intro2.htm",null,null);
pages[47][1]=new method("intro/intro3.htm","parent.content.go('')",true);

pages[48]=new Array();
pages[48][0]=new page("intro/intro3.htm",null,null);
pages[48][1]=new method("intro/intro4.htm","parent.content.go('')",true);

pages[49]=new Array();
pages[49][0]=new page("intro/intro4.htm",null,null);
pages[49][1]=new method("intro/intro5.htm","parent.content.go('')",true);

pages[50]=new Array();
pages[50][0]=new page("intro/intro5.htm",null,null);
pages[50][1]=new method("intro/intro6.htm","parent.content.go('')",true);

pages[51]=new Array();
pages[51][0]=new page("intro/intro6.htm",null,null);
pages[51][1]=new condition("needs1.htm","Mode Selection","ForceNew","yes");
pages[51][2]=new condition("useAcct.htm","Mode Selection","ForceExisting","yes");
pages[51][3]=new action("accounts.htm");

pages[52]=new Array();
pages[52][0]=new page("editfour.htm",null,null);
pages[52][1]=new method("register.htm","parent.content.go('register.htm')",true);
pages[52][2]=new method("ok.htm","parent.content.go('ok.htm')",true);
pages[52][3]=new method("okreboot.htm","parent.content.go('okreboot.htm')",true);
pages[52][4]=new method("error.htm","parent.content.go('error.htm')",true);



function checkShowIntroFlag()
{
	if ( parent && parent.parent && parent.parent.globals )
	{
		var theFile = parent.parent.globals.getAcctSetupFilename( self );	

		var theFlag = parent.parent.globals.GetNameValuePair( theFile, "Mode Selection", "Show_Intro_Screens" );
		if ( theFlag != null )
		{
			theFlag = theFlag.toLowerCase();
		}
		return ( theFlag == "yes" );
	}
	else
	{
		return false;
	}
}


// return the page number (according to ordinals along the path taken) of "file"
function findPageOffset( file )
{
	// ¥ take off any path information preceeding file name
	if ( ( slashPos = file.lastIndexOf( "/" ) ) > 0 )
	{
		file = file.substring( slashPos + 1, file.length );
	}

	// ¥ loop through all the page information and attempt to find a page name matching
	// "file"
	var	pageNum = -1;
	for ( var x = 0; x < pages.length; x++ )
	{
		for ( var y = 0; y < pages[ x ].length; y++ )
		{
			if ( pages[ x ][ y ].type== "INFO" )
			{
				var theName = pages[ x ][ y ].file;

				// ¥ again, remove any path
				if ( ( slashPos = theName.lastIndexOf( "/" ) ) > 0 )
				{
					theName = theName.substring( slashPos + 1, theName.length );
				}
		
				// ¥ pages[ x ][ y ].file
				if ( theName == file )
				{
					// ¥ found it
					pageNum = x;
				}
			
				// ¥ break here so we don't keep testing all the rest of the array entries after
				// we've found the "INFO" entry	
				break;
			}
		}
		
		// ¥ break if we've found it
		if ( pageNum >= 0 )
			break;
	}

	if ( parent && parent.parent && parent.parent.globals )
	{
		parent.parent.globals.debug( "\tfindPageOffset: '" +file+ "' is " + pageNum );
	}

	return pageNum;
}


// main navigational function in Account Setup, "msg" is typically the name of a button
// that was pressed
function go( msg )
{
	var editMode = false;

	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	if ( parent && parent.parent && parent.parent.globals )
	{
		editMode = ( parent.parent.globals.document.vars.editMode.value.toLowerCase() == "yes" ) ? true : false;
	}

//	var formName = new String(parent.content.location);
	var formName = "" + parent.content.location;

	if ( ( x = formName.lastIndexOf( "/" ) ) > 0 )
	{
		formName = formName.substring( x + 1, formName.length );
	}
	
	var pageNum = findPageOffset( formName );
	if ( pageNum >= 0 )
	{
		formName = pages[ pageNum ][ 0 ].file;
	}

//		var startingPageName = parent.content.document.forms[0].name;
	var startingPageName = formName;

	var thePlatform = new String( navigator.userAgent );
	var x = thePlatform.indexOf( "(" ) + 1;
	var y = thePlatform.indexOf( ";", x + 1 );
	thePlatform = thePlatform.substring( x, y );

	var separatorString = ",";

	// process messages

	if ( msg == "Next" )
	{
		// ¥ if something is wrong with the current page content, don't do anything
		if ( !parent || !parent.content || !parent.content.checkData )
			return;
			
		// ¥Êif we're currently loading, don't do anything
		if ( parent.content.loading )
			return;
			
		if ( editMode != true )
		{
			if ( parent.content.checkData() == false )
			{
				return;
			}
			// workaround for onunload handler bugs in 4.0b2; no longer using onunload handler
			if ( parent.content.saveData != null )
			{
				parent.content.saveData();
			}
		}

		var pageName = startingPageName;

		if ( parent && parent.parent && parent.parent.globals )
		{
			parent.parent.globals.debug("\ngo: Starting at page " +pageName);
		}

		var theFile = "";
		if ( parent && parent.parent && parent.parent.globals )
		{
			theFile = parent.parent.globals.getAcctSetupFilename( self );
		
		}
		var moved = false;
		var active = true;

		while ( active )
		{
			active = false;			
			var pageNum = findPageOffset( pageName );
			if ( pageNum < 0 )
			{
				alert( "The file '" + pageName + "' is unknown to Account Setup." );
				return;
			}

			for ( var x = 0; x < pages[ pageNum ].length; x++ )
			{
				if ( editMode == false && pages[ pageNum ][ x ].type == "CONDITION" )
				{
					var theFlag = parent.parent.globals.GetNameValuePair( theFile, pages[ pageNum ][ x ].section, 
																			pages[ pageNum ][ x ].variable );
					theFlag = theFlag.toLowerCase();
					
					if ( pages[ pageNum ][ x ].value == "..." )
					{
						if ( theFlag != null && theFlag != "" )
						{
							active = moved = true;
							pageName = pages[ pageNum ][ x ].file;
							break;
						}
					}
					else if ( theFlag == pages[ pageNum ][ x ].value )
					{
						active = moved = true;
						pageName = pages[ pageNum ][ x ].file;
						break;
					}
				}
	
				else if ( moved == false && pages[ pageNum ][ x ].type == "METHOD" )
				{
					var val = eval( pages[ pageNum ][ x ].method );
						
					if ( parent && parent.parent && parent.parent.globals )
					{
						parent.parent.globals.debug( "\tMethod: " + pages[ pageNum ][ x ].method );
						parent.parent.globals.debug( "\tReturned: " + val );
					}

					if ( val == pages[ pageNum ][ x ].value )
					{
						active = moved = true;
						pageName = pages[ pageNum ][ x ].file;
						break;
					}
				}

				else if ( moved == false && pages[ pageNum ][ x ].type == "ACTION" )
				{
					parent.parent.globals.debug( "\tAction: " + pages[ pageNum ][ x ].file );
					active = false;
					moved = true;
					pageName = pages[ pageNum ][ x ].file;
					break;
				}

			}
		}
		
		
		if ( pageName != startingPageName )
		{
			if ( parent && parent.parent && parent.parent.globals )
			{
				if ( parent.parent.globals.document.setupPlugin.NeedReboot() == true )
				{
					parent.parent.globals.forceReboot( pageName );
				}
				else
				{
					if ( pageName == "main.htm" )
					{
						parent.parent.globals.document.vars.pageHistory.value = "";
					}
					else
					{
						parent.parent.globals.document.vars.pageHistory.value += startingPageName + separatorString;
					}
	
					parent.parent.globals.debug( "go: Moving to page " + pageName );
	
					pages.current = pageName;
					parent.content.location.replace( pageName );

					if ( helpWindow && helpWindow != null )
					{
						if ( helpWindow.closed == false )
						{
							doHelp( pageName );
						}
					}
				}
			}
			else
			{
				pages.current = pageName;

				if ( pages[ pageNum ][ 0 ].top == true )
				{
					parent.location.replace( pageName );
				}
				else
				{
					var theLoc = "" + parent.content.location;
					if ( ( x = theLoc.lastIndexOf( "/" ) ) > 0 )
					{
						pageName = theLoc.substring( 0, x + 1 ) + pageName;
					}
					
					parent.content.location.replace( pageName );
				}
			}
		}
	}

	else if (msg == "Back")	{
		if (parent.content.loading) return;

		// workaround for onunload handler bugs in 4.0b2; no longer using onunload handler
		if (parent.content.saveData!=null)	{
			parent.content.saveData();
			}
//		parent.content.history.back();

		if (parent.content.verifyData)	{
			var verifyFlag = parent.content.verifyData();
			if (verifyFlag != true)	{
				generateControls();
				return;
				}
			}

		if (parent && parent.parent && parent.parent.globals)	{
			var historyCleanup = true;
			while (historyCleanup == true)	{
				historyCleanup = false;

				var pageHistory = parent.parent.globals.document.vars.pageHistory.value;
				if (pageHistory!="")	{			
					var pageName="";
					x = pageHistory.lastIndexOf(separatorString);
					pageHistory=pageHistory.substring(0,x);
					x = pageHistory.lastIndexOf(separatorString);
					if (x>=0)	{
						pageName=pageHistory.substring(x+1,pageHistory.length);
						parent.parent.globals.document.vars.pageHistory.value = pageHistory.substring(0,x+1);
						if ((pageName == "register.htm") || (pageName == "error.htm"))	{
							historyCleanup = true;
							}
						}
					else	{
						pageName=pageHistory;
						parent.parent.globals.document.vars.pageHistory.value="";
						}
					}
				}
			parent.parent.globals.debug("go: Moving back to page " +pageName);
			
			if (pageName == "undefined")
				return;

			pages.current = pageName;
			if (pageNum >= 0 && pages[pageNum][0].fullhistory == true)	{
				parent.parent.history.back();
				}
			else	{
				parent.content.location.replace(pageName);
				}

			if (helpWindow && helpWindow != null)	{
				if (helpWindow.closed==false)	{
					doHelp(pageName);
					}
				}
			}
		else	{
			if (pages[pageNum][0].fullhistory == true)	{
				parent.parent.history.back();
				}
			else	{
				parent.content.history.back();
				}
			}
		}
	else if (msg == "Help")	{
		doHelp(formName);
		}
	else if (msg == "Show Screen")	{
		var pageNum=findPageOffset(formName);
		if (pageNum>=0)	{
			var section=pages[pageNum][0].section;
			var variable=pages[pageNum][0].variable;
			if (section!=null && section!="" && variable!=null && variable!="")	{
				showScreenToggle=true;
				var theFile = parent.parent.globals.getAcctSetupFilename(self);
				var theFlag = parent.parent.globals.GetNameValuePair(theFile,section, variable);
				theFlag = theFlag.toLowerCase();
				if (theFlag == "no")	theFlag="yes";
				else			theFlag="no";
				
				parent.parent.SetNameValuePair(theFile,section, variable,theFlag);
				}
			}
		}
	else if (msg == "Later")	{
		if (parent.content.go("Later") == true)	{

			if ((parent.parent.globals.document.vars.editMode.value.toLowerCase() != "yes") || (confirm("Normally, this would complete the Account Setup process and quit Communicator.  Would you like to quit now?") == true))
				{
//					parent.content.location.href = "later.htm";
					if (parent && parent.parent && parent.parent.globals)	{
						if (parent.parent.globals.document.vars.editMode.value.toLowerCase() != "yes")
							parent.parent.globals.saveGlobalData();
						parent.parent.globals.document.setupPlugin.QuitNavigator();
						}
					window.close();
				}
			}
		}
	else if (msg == "Done")	{
		if (parent && parent.parent && parent.parent.globals)	{
			if (parent.parent.globals.document.vars.editMode.value.toLowerCase() != "yes")
				parent.parent.globals.saveGlobalData();
			if ((parent.parent.globals.document.vars.editMode.value.toLowerCase() != "yes") || (confirm("Normally, this would complete the Account Setup process and quit Communicator.  Would you like to quit now?") == true)) {	
				parent.parent.globals.document.setupPlugin.QuitNavigator();
				window.close();
				}
			}
		}
	else if (msg == "Exit")	{
		var	longMsgFlag = true;
		var confirmFlag = false;
		
		if (formName.indexOf("main.htm")>=0)			longMsgFlag = false;
		else if (formName.indexOf("aboutbox.htm")>=0)	longMsgFlag = false;
		else if (formName.indexOf("error.htm")>=0)		longMsgFlag = false;
		else if (formName.indexOf("intro/")>=0)			longMsgFlag = false;
		else if (formName.indexOf("ipreview/")>=0)		longMsgFlag = false;
		else if (formName.indexOf("preview/")>=0)		longMsgFlag = false;

		if (longMsgFlag == true)	{
			if (parent.parent.globals.document.vars.editMode.value.toLowerCase() != "yes")	
				confirmFlag = confirm("Your haven't finished setting up your account. Are you sure you want to quit Account Setup?");
			else	// this is for the account setup editor
				confirmFlag = confirm("Are you sure you want to quit the Account Setup Editor?");
			}
		else	{
			confirmFlag = confirm("Quit Account Setup?");
			}

		if (confirmFlag == true)	{
			if (parent && parent.parent && parent.parent.globals)	{
				parent.parent.globals.saveGlobalData();
				parent.parent.globals.document.setupPlugin.QuitNavigator();
				}
			window.close();
			}
		}
	else if (msg == "Restart")	{
		if (parent.parent.globals.document.vars.editMode.value.toLowerCase() != "yes")	{
			parent.parent.globals.saveGlobalData();
			parent.parent.globals.document.setupPlugin.Reboot(null);
			window.close();
			}
		else	{
			alert("Cannot reboot in edit mode.");
			}
		}
	else if (msg == "About")	{
		parent.parent.globals.document.vars.pageHistory.value += startingPageName + separatorString;
		pages.current = "aboutbox.htm";
		parent.content.location.replace("aboutbox.htm");
		}
	else if (msg == "Setup")	{
		parent.parent.globals.document.vars.pageHistory.value += startingPageName + separatorString;

		var acctSetupFile = parent.parent.globals.getAcctSetupFilename(self);
		var newPathFlag = parent.parent.globals.GetNameValuePair(acctSetupFile,"Mode Selection","ForceNew");
		newPathFlag = newPathFlag.toLowerCase();
		var existingPathFlag = parent.parent.globals.GetNameValuePair(acctSetupFile,"Mode Selection","ForceExisting");
		existingPathFlag = existingPathFlag.toLowerCase();

		var pageName="";
		if (newPathFlag == "yes" && existingPathFlag != "yes")	{
			pageName = "needs1.htm";
			}
		else if (existingPathFlag == "yes" && newPathFlag != "yes")	{
			pageName = "useAcct.htm";
			}
		else	{
			pageName = "accounts.htm";
			}
		parent.content.location.replace(pageName);
		if (helpWindow && helpWindow != null)	{
			if (helpWindow.closed==false)	{
				doHelp(pageName);
				}
			}
		}
	else if (msg == "Edit Settings")	{
		parent.parent.globals.document.vars.pageHistory.value += startingPageName + separatorString;
		pages.current = "settings.htm";
		parent.content.location.replace("../CG/docs/settings.htm");
		}
	else if (msg == "Manage Servers")	{
		parent.parent.globals.document.vars.pageHistory.value += startingPageName + separatorString;
		pages.current = "editregs.htm";
		parent.content.location.replace("../CG/docs/editregs.htm");
		}
	else if (msg == "Manage Accounts")	{
		parent.parent.globals.document.vars.pageHistory.value += startingPageName + separatorString;
		pages.current = "editisps.htm";
		parent.content.location.replace("../CG/docs/editisps.htm");
		}
	else if (msg == "Edit IAS")			{
		if (thePlatform == "Macintosh")
			parent.parent.globals.document.vars.pageHistory.value += "../../Tools/CG/docs/" + startingPageName + separatorString;
		else
			parent.parent.globals.document.vars.pageHistory.value += "../../../AccountSetupTools/CG/docs/" + startingPageName + separatorString;
			
		pages.current = "addias.htm";
		parent.content.location.replace("ias/addias.htm");
		}
	else if (msg == "Edit NCI")			{
		if (thePlatform == "Macintosh")
			parent.parent.globals.document.vars.pageHistory.value += "../../Tools/CG/docs/" + startingPageName + separatorString;
		else
			parent.parent.globals.document.vars.pageHistory.value += "../../../AccountSetupTools/CG/docs/" + startingPageName + separatorString;
		pages.current = "addnci.htm";
		parent.content.location.replace("nci/addnci.htm");
		}	
	else if (msg == "Screen Options")	{
		parent.parent.globals.document.vars.pageHistory.value += startingPageName + separatorString;
			
		if (formName == "namepw.htm")	{
				pages.current = "asktty.htm";
				parent.content.location.replace("../CG/docs/asktty.htm");
				}
		//else if (formName == "servers.htm")	{
		//		pages.current = "askserv.htm";
		//		parent.content.location.replace("../CG/docs/askserv.htm");
		//		}
		else if (formName == "ok.htm")	{
				pages.current = "asksvinf.htm";
				parent.content.location.replace("../CG/docs/asksvinf.htm");
				}
		else if (formName == "okreboot.htm")	{
				pages.current = "asksvinf.htm";
				parent.content.location.replace("../CG/docs/asksvinf.htm");
				}
		else if (formName == "billing.htm") {
				pages.current = "editcc.htm";
				parent.content.location.replace("../CG/docs/editcc.htm");
				}
		else if (formName == "main.htm")	{
				pages.current = "settings.htm";
				parent.content.location.replace("../CG/docs/settings.htm");
				}
		else if (formName == "useAcct.htm")	{
				pages.current = "editisps.htm";
				parent.content.location.replace("../CG/docs/editisps.htm");
				}
		else if (formName == "connect1.htm") {
				pages.current = "editregs.htm";
				parent.content.location.replace("../CG/docs/editregs.htm");
				}		

		}	//end screen options special casing
		
	else	
		parent.content.go(msg);
//	generateControls();
}



function loadData()
{
	var file="";
		
//	preLoadImages();

	if (parent && parent.parent && parent.parent.globals)	{
		file = parent.parent.globals.document.vars.startupFile.value;
		}
	if (file != null && file != "")	{
		if (parent && parent.parent && parent.parent.globals)	{
			parent.parent.globals.document.vars.startupFile.value = "";
			}
		parent.content.location.replace(file);
		}
	generateControls();
}



// end hiding contents from old browsers  -->
