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

var	globals = parent.parent.globals;
var plugin = globals.document.setupPlugin;
var documentVars = globals.document.vars;

function go(msg)
{
	if (parent.parent.globals.document.vars.editMode.value == "yes")
		return true;
	else
		return checkData();
}

function checkData()
{
	return true;
}

function loadData()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	if ( parent.controls.generateControls )
		parent.controls.generateControls();

	if ( globals.document.vars.editMode.value != "yes" )
	{		// only connect to reggie if editmode is off

		var connectStatusFlag = globals.document.setupPlugin.IsDialerConnected();
		if ( connectStatusFlag == true )
		{
			if ( confirm( "Account Setup can't connect until you close your current connection. Close the connection now?" ) == false )
				return;
			globals.document.setupPlugin.DialerHangup();
		}

		var acctSetupFile = globals.getAcctSetupFilename( self );
		var regFile = globals.getFolder( self ) + "isp/en/" + globals.selectedISP + "/client_data/config/config.ias.r";
		
		// determine name of scripting file
		var scriptEnabledFlag = "FALSE";
		var scriptFile = globals.GetNameValuePair( regFile, "Dial-In Configuration", "ScriptFileName" );
		if ( scriptFile != null && scriptFile != "" )
		{
			scriptFile = acctSetupFolder + scriptFile;
			scriptEnabledFlag = "TRUE";
		}
		else
		{
			scriptFile = "";
			scriptEnabledFlag = "FALSE";
		}
	
	
		// determine tty
		var ttyFlag = globals.GetNameValuePair( regFile, "Security", "SecurityDevice" );
		ttyFlag = ttyFlag.toLowerCase();
		if ( ttyFlag == "yes" )
			ttyFlag = "TRUE";
		else
			ttyFlag = "FALSE";
			
		// determine outside line access string	
		var outsideLineAccessStr = "";
		if ( globals.document.vars.prefixData.value != "" )
		{
			outsideLineAccessStr = parent.parent.globals.document.vars.prefixData.value;
			x = outsideLineAccessStr.indexOf(",");
			if ( x < 0 )
				outsideLineAccessStr = outsideLineAccessStr + ",";
		}
	
	
		// configure dialer for Registration Server	
		dialerData = globals.document.setupPlugin.newStringArray(28);		// increment this # as new dialer strings are added
		dialerData[0]  = "FileName=" + regFile;
		dialerData[1]  = "AccountName=" + globals.GetNameValuePair( regFile,"Dial-In Configuration","SiteName");
		dialerData[2]  = "ISPPhoneNum=" + globals.GetNameValuePair( regFile,"Dial-In Configuration","Phone");
		dialerData[3]  = "LoginName=" + globals.GetNameValuePair( regFile,"Dial-In Configuration","Name");
		dialerData[4]  = "Password=" + globals.GetNameValuePair( regFile,"Dial-In Configuration","Password");
		dialerData[5]  = "DNSAddress=" + globals.GetNameValuePair( regFile,"IP","DNSAddress");
		dialerData[6]  = "DNSAddress2=" + globals.GetNameValuePair( regFile,"IP","DNSAddress2");
		dialerData[7]  = "DomainName=" + globals.GetNameValuePair( regFile,"IP","DomainName");
		dialerData[8]  = "IPAddress=" + globals.GetNameValuePair( regFile,"IP","IPAddress");
		dialerData[9]  = "IntlMode=" + ((intlFlag=="yes") ? "TRUE":"FALSE");
		dialerData[10] = "DialOnDemand=TRUE";
		dialerData[11] = "ModemName=" + globals.document.vars.modem.value;
		dialerData[12] = "ModemType=" + globals.document.setupPlugin.GetModemType(parent.parent.globals.document.vars.modem.value);
		dialerData[13] = "DialType=" + globals.document.vars.dialMethod.value;
		dialerData[14] = "OutsideLineAccess=" + outsideLineAccessStr;
		dialerData[15] = "DisableCallWaiting=" + ( ( globals.document.vars.cwData.value != "" ) ? "TRUE" : "FALSE");
		dialerData[16] = "DisableCallWaitingCode=" + globals.document.vars.cwData.value;
		dialerData[17] = "UserAreaCode=" + globals.document.vars.modemAreaCode.value;				// XXX what to do if international mode?
		dialerData[18] = "CountryCode=" + globals.document.vars.countryCode.value;
		dialerData[19] = "LongDistanceAccess=1";									// XXX
		dialerData[20] = "DialAsLongDistance=TRUE";									// XXX
		dialerData[21] = "DialAreaCode=TRUE";										// XXX
		dialerData[22] = "ScriptEnabled=" + scriptEnabledFlag;
		dialerData[23] = "ScriptFileName=" + scriptFile;
		dialerData[24] = "NeedsTTYWindow=" + ttyFlag;
		dialerData[25] = "Location=Home";
	 	dialerData[26] = "DisconnectTime=" + globals.GetNameValuePair( acctSetupFile, "Mode Selection", "Dialer_Disconnect_After" );
		dialerData[27] = "Path=Server";
	
		// write out dialer data to Java Console
		if ( globals.document.vars.debugMode.value.toLowerCase() == "yes")
		{
			globals.debug( "\nDialer data (for Registration Server): " );
			var numElements = dialerData.length;
			for ( var x = 0; x < numElements; x++ )
				globals.debug("        " + x + ": " + dialerData[ x ]);
		}
	
		globals.document.setupPlugin.DialerConfig( dialerData, true );
	
		// check if we need to reboot
		
		if ( globals.document.setupPlugin.NeedReboot() == true )
		{
			globals.forceReboot( "2step.htm" );		// XXX hardcode in name of next screen???
			return;
		}
	
		if ( globals.document.setupPlugin.DialerConnect() == false )
		{
			globals.document.setupPlugin.DialerHangup();
			window.location.replace( "error.htm" );							// XXX hardcode in name of next screen???
			return;
		}
	
	
		// get platform (XXX Macintosh should not be uppercased?)
		platform = globals.getPlatform();
		
		if ( platform == "Win16" )
			platform = "WIN31";
		else if ( platform == "Win95" )
			platform = "WIN95";
		else if ( platform == "WinNT" )
			platform = "WINNT";
	
		// get registration server reference
	/*
		var theFolder = parent.parent.globals.getConfigFolder(self);
		var theRegFile = parent.parent.globals.GetNameValuePair(theFile,"New Acct Mode","RegServer");
		var theRegServer = "";
		if (theRegFile == null || theRegFile == "")	{
			theRegFile = parent.parent.globals.document.vars.regServer.value;
			}
		if (theRegFile == null || theRegFile == "")	{
			alert("Internal problem determining the Registration Server.");
			return;
			}	
		theRegFile = theFolder + theRegFile;
	*/
		regServer = globals.GetNameValuePair( regFile, "IP", "RegCGI" );
	
		if ( regServer == null || regServer == "" )
		{
			alert( "Internal problem determining the Registration Server." );
			return;
		}
		
		// build TAPI phone number
		var theHomePhone="";
		var thePhone="";
		var theCountry="";
		var intlFlag = parent.parent.globals.GetNameValuePair( acctSetupFile, "Mode Selection", "IntlMode" );
		intlFlag = intlFlag.toLowerCase();
		if ( intlFlag == "yes" )
		{
			theHomePhone = globals.document.vars.phoneNumber.value;
			thePhone = globals.document.vars.modemPhoneNumber.value;
			theCountry = globals.document.vars.country.value;
		}
		else
		{
			theHomePhone = "(" + globals.document.vars.areaCode.value + ") " + globals.document.vars.phoneNumber.value;
			thePhone = "(" + globals.document.vars.modemAreaCode.value + ") " + globals.document.vars.modemPhoneNumber.value;
			theCountry = "USA";
		}
	
		// mangle year and month for submission to registration server
	
		var year = parseInt( globals.document.vars.year.value );
		while ( year >= 100 )
			year -= 100;
		if ( year < 10 )
			yeahr = "0" + year;
	
		var month = parseInt( globals.document.vars.month.value );
		month += 1;
	
	// the following values are commented out as the values are set in HTML
	/*
		document.forms[0].RS_FORM_TYPE.value			= "custinfo";
		document.forms[0].REG_SETUP_NAME.value			= "Account Setup";
		document.forms[0].REG_SIGNUP_VERSION.value		= "4.0";
		document.forms[0].REG_SCRIPTING.value			= "Yes";
		document.forms[0].REG_SOURCE.value				= "test";
		document.forms[0].REG_CHANNEL.value				= "";
	*/
	
		// fill out custinfo form elements, then submit
	
		document.forms[0].REG_PLATFORM.value			= platform;
		document.forms[0].AS_INTLMODE.value				= intlFlag;
	
		document.forms[0].CST_LAST_NAME.value			= globals.document.vars.last.value;
		document.forms[0].CST_FIRST_NAME.value			= globals.document.vars.first.value;
		document.forms[0].CST_ORGANIZATION_NAME.value	= globals.document.vars.company.value;
		document.forms[0].CST_STREET_1.value			= globals.document.vars.address1.value;
		document.forms[0].CST_STREET_2.value			= globals.document.vars.address2.value;
		document.forms[0].CST_STREET_3.value			= globals.document.vars.address3.value;
		document.forms[0].CST_CITY.value				= globals.document.vars.city.value;
		document.forms[0].CST_STATE_PROVINCE.value		= globals.document.vars.state.value;
		document.forms[0].CST_POSTAL_CODE.value			= globals.document.vars.zip.value;
		document.forms[0].CST_COUNTRY.value				= theCountry;
		document.forms[0].CST_PHONE.value				= thePhone;
		document.forms[0].CST_HOMEPHONE.value			= theHomePhone;		
		document.forms[0].CST_CC_NO.value				= globals.document.vars.cardnumber.value;
		document.forms[0].CST_CC_TYPE.value				= globals.document.vars.cardcode.value;
		document.forms[0].CST_CC_MTH_EXPIRE.value		= month;
		document.forms[0].CST_CC_YEAR_EXPIRE.value		= year;
		document.forms[0].CST_CC_CARDHOLDER.value		= globals.document.vars.cardname.value;
		document.forms[0].CST_JUNK_MAIL.value			= "";			// XXX
	
		document.forms[0].action = regServer;
		
		// for win32 platforms make sure we don't get ISP's with scripting
		if ( ( platform == "WIN95" ) || ( platform == "WINNT" ) )
			document.forms[0].REG_SCRIPTING.value		= "No";

		// declare animation support for appropriate platform
		if ( platform == "Macintosh" )
		{
			document.forms[0].AS_MAC_ANIMATION_SUPPORT.value = "YES";
			document.forms[0].AS_WIN_ANIMATION_SUPPORT.value = "NO";
		}
		else
		{
			document.forms[0].AS_MAC_ANIMATION_SUPPORT.value = "NO";
			document.forms[0].AS_WIN_ANIMATION_SUPPORT.value = "YES";
		}


		// write out Milan data to Java Console
		if ( globals.document.vars.debugMode.value.toLowerCase() == "yes" )
		{
			globals.debug( "\nRegServer (Milan) data: " + document.forms[ 0 ].action );
			var numElements = document.forms[ 0 ].length;
			for ( var x = 0; x < document.forms[ 0 ].length; x++ )
				globals.debug( "        " + x + ": " + document.forms[ 0 ].elements[ x ].name + "=" + document.forms[ 0 ].elements[ x ].value );
		}
	
		// submit Milan data
		navigator.preference( "security.warn_submit_insecure", false );
		navigator.preference( "security.warn_entering_secure", false );
		navigator.preference( "security.warn_leaving_secure", false );

		parent.parent.globals.setRegisterMode( 1 );
		document.forms[ 0 ].submit();										// automatically submit form to registration server
	}
}

function saveData()
{
}

// end hiding contents from old browsers  -->
