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



function go( msg )
{
	if ( parent.parent.globals.document.vars.editMode.value == "yes" )
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

	// ¥ only connect to reggie if editmode is off
	if ( parent.parent.globals.document.vars.editMode.value != "yes" )
	{
		var connectStatusFlag = parent.parent.globals.document.setupPlugin.IsDialerConnected();
		if ( connectStatusFlag == true )
		{
			if ( confirm( "Account Setup can't connect until you close your current connection. Close the connection now?" ) == false )
			{
				return;
			}
			parent.parent.globals.document.setupPlugin.DialerHangup();
		}

		var theFolder = parent.parent.globals.getConfigFolder( self );
		var theFile = parent.parent.globals.getAcctSetupFilename( self );
		var theRegFile = theFolder + parent.parent.globals.document.vars.regServer.value;
		var intlFlag = parent.parent.globals.GetNameValuePair( theFile, "Mode Selection", "IntlMode" );
		intlFlag = intlFlag.toLowerCase();
				
		// ¥ determine name of scripting file
		var scriptEnabledFlag = "FALSE";
		var theScriptFile = parent.parent.globals.GetNameValuePair( theRegFile, "Dial-In Configuration", "ScriptFileName" );
		if ( theScriptFile != null && theScriptFile != "" )
		{
			theScriptFile = theFolder + theScriptFile;
			scriptEnabledFlag = "TRUE";
		}
		else
		{
			theScriptFile = "";
			scriptEnabledFlag = "FALSE";
		}
	
		// ¥ determine tty
		var ttyFlag = parent.parent.globals.GetNameValuePair( theRegFile, "Security", "SecurityDevice" );
		ttyFlag = ttyFlag.toLowerCase();
		if ( ttyFlag == "yes" )
		{
			ttyFlag = "TRUE";
		}
		else
		{
			ttyFlag = "FALSE";
		}
	
		
		// ¥ determine outside line access string
		var outsideLineAccessStr = "";
		if ( parent.parent.globals.document.vars.prefixData.value != "" )
		{
			outsideLineAccessStr = parent.parent.globals.document.vars.prefixData.value;
			x = outsideLineAccessStr.indexOf( "," );
			if ( x < 0 )
				outsideLineAccessStr = outsideLineAccessStr + ",";
		}
	
	
		// ¥ configure dialer for Registration Server
		dialerData = parent.parent.globals.document.setupPlugin.newStringArray( 28 );		// increment this # as new dialer strings are added
		dialerData[ 0 ]  = "FileName=" + theRegFile;
		dialerData[ 1 ]  = "AccountName=" + parent.parent.globals.GetNameValuePair( theRegFile, "Dial-In Configuration", "SiteName" );
		dialerData[ 2 ]  = "ISPPhoneNum=" + parent.parent.globals.GetNameValuePair( theRegFile, "Dial-In Configuration", "Phone" );
		dialerData[ 3 ]  = "LoginName=" + parent.parent.globals.GetNameValuePair( theRegFile, "Dial-In Configuration", "Name" );
		dialerData[ 4 ]  = "Password=" + parent.parent.globals.GetNameValuePair( theRegFile, "Dial-In Configuration", "Password" );
		dialerData[ 5 ]  = "DNSAddress=" + parent.parent.globals.GetNameValuePair( theRegFile, "IP", "DNSAddress" );
		dialerData[ 6 ]  = "DNSAddress2=" + parent.parent.globals.GetNameValuePair( theRegFile, "IP", "DNSAddress2" );
		dialerData[ 7 ]  = "DomainName=" + parent.parent.globals.GetNameValuePair( theRegFile, "IP", "DomainName" );
		dialerData[ 8 ]  = "IPAddress=" + parent.parent.globals.GetNameValuePair( theRegFile, "IP", "IPAddress" );
		dialerData[ 9 ]  = "IntlMode=" + ( ( intlFlag == "yes" ) ? "TRUE" : "FALSE" );
		dialerData[ 10 ] = "DialOnDemand=TRUE";
		dialerData[ 11 ] = "ModemName=" + parent.parent.globals.document.vars.modem.value;
		dialerData[ 12 ] = "ModemType=" + parent.parent.globals.document.setupPlugin.GetModemType( parent.parent.globals.document.vars.modem.value );
		dialerData[ 13 ] = "DialType=" + parent.parent.globals.document.vars.dialMethod.value;
		dialerData[ 14 ] = "OutsideLineAccess=" + outsideLineAccessStr;
		dialerData[ 15 ] = "DisableCallWaiting=" + ( ( parent.parent.globals.document.vars.cwData.value != "" ) ? "TRUE" : "FALSE" );
		dialerData[ 16 ] = "DisableCallWaitingCode=" + parent.parent.globals.document.vars.cwData.value;
		dialerData[ 17 ] = "UserAreaCode=" + parent.parent.globals.document.vars.modemAreaCode.value;				// XXX what to do if international mode?
		dialerData[ 18 ] = "CountryCode=" + parent.parent.globals.document.vars.countryCode.value;
		dialerData[ 19 ] = "LongDistanceAccess=1";									// XXX
		dialerData[ 20 ] = "DialAsLongDistance=TRUE";									// XXX
		dialerData[ 21 ] = "DialAreaCode=TRUE";										// XXX
		dialerData[ 22 ] = "ScriptEnabled=" + scriptEnabledFlag;
		dialerData[ 23 ] = "ScriptFileName=" + theScriptFile;
		dialerData[ 24 ] = "NeedsTTYWindow=" + ttyFlag;
		dialerData[ 25 ] = "Location=Home";
	 	dialerData[ 26 ] = "DisconnectTime=" + parent.parent.globals.GetNameValuePair( theFile, "Mode Selection", "Dialer_Disconnect_After" );
		dialerData[ 27 ] = "Path=Server";
	
		// ¥ write out dialer data to Java Console
		if ( parent.parent.globals.document.vars.debugMode.value.toLowerCase() == "yes" )
		{
			parent.parent.globals.debug( "\nDialer data (for Registration Server): " );
			var numElements = dialerData.length;
			for ( var x = 0; x < numElements; x++ )
			{
				parent.parent.globals.debug( "        " + x + ": " + dialerData[ x ] );
			}
		}
	
		// ¥ configure the dialer
		parent.parent.globals.document.setupPlugin.DialerConfig( dialerData, true );
	
		// ¥ check if we need to reboot
		if ( parent.parent.globals.document.setupPlugin.NeedReboot() == true )
		{
			parent.parent.globals.forceReboot( "1step.htm" );		// XXX hardcode in name of next screen???
			return;
		}
	
		if ( parent.parent.globals.document.setupPlugin.DialerConnect() == false )
		{
			parent.parent.globals.document.setupPlugin.DialerHangup();
			window.location.replace( "error.htm" );							// XXX hardcode in name of next screen???
			return;
		}
	
		
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
		theRegServer = parent.parent.globals.GetNameValuePair( theRegFile, "IP", "RegCGI" );
	
		if ( theRegServer == null || theRegServer == "" )
		{
			alert( "Internal problem determining the Registration Server." );
		}
	
		// the following values are commented out as the values are set in HTML
	
	/*
		document.forms[0].RS_FORM_TYPE.value			= "custinfo";
		document.forms[0].REG_SETUP_NAME.value			= "Account Setup";
		document.forms[0].REG_SIGNUP_VERSION.value		= "4.0";
		document.forms[0].REG_SCRIPTING.value			= "Yes";
		document.forms[0].REG_SOURCE.value				= "test";
		document.forms[0].REG_CHANNEL.value				= "";
	*/
	
	
		// ¥ fill out custinfo form elements, then submit
		document.forms[ 0 ].CST_PHONE_NUMBER.value 		= parent.parent.globals.document.vars.modemPhoneNumber.value;
		document.forms[ 0 ].CST_AREA_CODE_1.value		= parent.parent.globals.document.vars.modemAreaCode.value;
		document.forms[ 0 ].CST_AREA_CODE_2.value		= parent.parent.globals.document.vars.altAreaCode1.value;
		document.forms[ 0 ].CST_AREA_CODE_3.value		= parent.parent.globals.document.vars.altAreaCode2.value;
		document.forms[ 0 ].CST_AREA_CODE_4.value		= parent.parent.globals.document.vars.altAreaCode3.value;
		document.forms[ 0 ].CLIENT_LANGUAGE.value		= navigator.language;
		document.forms[ 0 ].REG_SOURCE.value			= "APL";

		//document.forms[ 0 ].action = theRegServer;
		
//		// ¥ for win32 platforms make sure we don't get ISP's with scripting
//		if ( ( thePlatform == "WIN95" ) || ( thePlatform == "WINNT" ) )
//		{
//			document.forms[ 0 ].REG_SCRIPTING.value		= "No";
//		}

//		// ¥ declare animation support for appropriate platform
//		if ( thePlatform == "Macintosh" )
//		{
//			document.forms[ 0 ].AS_MAC_ANIMATION_SUPPORT.value		= "YES";
//			document.forms[ 0 ].AS_WIN_ANIMATION_SUPPORT.value		= "NO";
//		}
//		else
//		{
//			document.forms[ 0 ].AS_MAC_ANIMATION_SUPPORT.value		= "NO";
//			document.forms[ 0 ].AS_WIN_ANIMATION_SUPPORT.value		= "YES";
//		}


		// ¥ write out reggie data to Java Console
		if ( parent.parent.globals.document.vars.debugMode.value.toLowerCase() == "yes" )
		{
			parent.parent.globals.debug( "\nRegServer data: " + document.forms[ 0 ].action );
			var numElements = document.forms[ 0 ].length;
			for ( var x = 0; x < document.forms[ 0 ].length; x++ )
			{
				parent.parent.globals.debug( "        " + x + ": " + document.forms[ 0 ].elements[ x ].name + "=" + document.forms[ 0 ].elements[ x ].value );
			}
		}
	
		// ¥ submit reggie data
		navigator.preference( "security.warn_submit_insecure", false );
		navigator.preference( "security.warn_entering_secure", false );
		navigator.preference( "security.warn_leaving_secure", false );

		// ¥ automatically submit form to registration server
		//document.forms[ 0 ].submit();

		reggieData = parent.parent.globals.document.setupPlugin.newStringArray( 7 );		// increment this # as new dialer strings are added
		reggieData[ 0 ] = "CST_PHONE_NUMBER=" + parent.parent.globals.document.vars.modemPhoneNumber.value;
		reggieData[ 1 ] = "REG_SOURCE=" + "APL";
		reggieData[ 2 ] = "CLIENT_LANGUAGE=" + navigator.language;
		reggieData[ 3 ] = "CST_AREA_CODE_1=" + parent.parent.globals.document.vars.modemAreaCode.value;
		reggieData[ 4 ] = "CST_AREA_CODE_2=" + parent.parent.globals.document.vars.altAreaCode1.value;
		reggieData[ 5 ] = "CST_AREA_CODE_3=" + parent.parent.globals.document.vars.altAreaCode2.value;
		reggieData[ 6 ] = "CST_AREA_CODE_4=" + parent.parent.globals.document.vars.altAreaCode3.value;
		
		parent.parent.globals.setRegisterMode( 1 );
		if ( parent.parent.globals.document.setupPlugin.GenerateComparePage( "https://reggie.netscape.com/IAS5/docs/reg.cgi", reggieData ) == true )
			window.location.replace( "compare.html" );
		else
			window.location.replace( "error.htm" );
	}
}



function saveData()
{
}



// end hiding contents from old browsers  -->
