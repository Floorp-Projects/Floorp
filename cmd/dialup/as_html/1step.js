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

function go( msg )
{
	parent.parent.globals.debug( "1step go" );
	
	if ( parent.parent.globals.document.vars.editMode.value == "yes" )
		return true;
	else
		return checkData();
}

function checkData()
{
	return true;
}

function configureDialer( configFolder, acctSetupIni, regFile )
{
	globals.debug( "Configuring dialer" );

	var		intlFlag = globals.GetNameValuePair( acctSetupIni, "Mode Selection", "IntlMode" );
	intlFlag = intlFlag.toLowerCase();
	
	// * determine name of scripting file
	var		scriptEnabledFlag = "FALSE";
	var		scriptFile = globals.GetNameValuePair( regFile, "Dial-In Configuration", "ScriptFileName" );
	if ( scriptFile != null && scriptFile != "" )
	{
		scriptFile = configFolder + scriptFile;
		scriptEnabledFlag = "TRUE";
	}
	else
	{
		scriptFile = "";
		scriptEnabledFlag = "FALSE";
	}

	// * determine tty
	var 	ttyFlag = globals.GetNameValuePair( regFile, "Security", "SecurityDevice" );
	ttyFlag = ttyFlag.toLowerCase();
	if ( ttyFlag == "yes" )
		ttyFlag = "TRUE";
	else
		ttyFlag = "FALSE";

	
	// * determine outside line access string
	var		outsideLineAccessStr = "";
	if ( documentVars.prefixData.value != "" )
	{
		outsideLineAccessStr = documentVars.prefixData.value;
		x = outsideLineAccessStr.indexOf( "," );
		if ( x < 0 )
			outsideLineAccessStr = outsideLineAccessStr + ",";
	}


	// * configure dialer for Registration Server
	dialerData = plugin.newStringArray( 28 );		// increment this # as new dialer strings are added
	dialerData[ 0 ]  = "FileName=" + regFile;
	dialerData[ 1 ]  = "AccountName=" + globals.GetNameValuePair( regFile, "Dial-In Configuration", "SiteName" );
	dialerData[ 2 ]  = "ISPPhoneNum=" + globals.GetNameValuePair( regFile, "Dial-In Configuration", "Phone" );
	dialerData[ 3 ]  = "LoginName=" + globals.GetNameValuePair( regFile, "Dial-In Configuration", "Name" );
	dialerData[ 4 ]  = "Password=" + globals.GetNameValuePair( regFile, "Dial-In Configuration", "Password" );
	dialerData[ 5 ]  = "DNSAddress=" + globals.GetNameValuePair( regFile, "IP", "DNSAddress" );
	dialerData[ 6 ]  = "DNSAddress2=" + globals.GetNameValuePair( regFile, "IP", "DNSAddress2" );
	dialerData[ 7 ]  = "DomainName=" + globals.GetNameValuePair( regFile, "IP", "DomainName" );
	dialerData[ 8 ]  = "IPAddress=" + globals.GetNameValuePair( regFile, "IP", "IPAddress" );
	dialerData[ 9 ]  = "IntlMode=" + ( ( intlFlag == "yes" ) ? "TRUE" : "FALSE" );
	dialerData[ 10 ] = "DialOnDemand=TRUE";
	dialerData[ 11 ] = "ModemName=" + globals.document.vars.modem.value;
	dialerData[ 12 ] = "ModemType=" + plugin.GetModemType( documentVars.modem.value );
	dialerData[ 13 ] = "DialType=" + documentVars.dialMethod.value;
	dialerData[ 14 ] = "OutsideLineAccess=" + outsideLineAccessStr;
	dialerData[ 15 ] = "DisableCallWaiting=" + ( ( documentVars.cwData.value != "" ) ? "TRUE" : "FALSE" );
	dialerData[ 16 ] = "DisableCallWaitingCode=" + documentVars.cwData.value;
	dialerData[ 17 ] = "UserAreaCode=" + documentVars.modemAreaCode.value;				// XXX what to do if international mode?
	dialerData[ 18 ] = "CountryCode=" + documentVars.countryCode.value;
	dialerData[ 19 ] = "LongDistanceAccess=1";									// XXX
	dialerData[ 20 ] = "DialAsLongDistance=TRUE";									// XXX
	dialerData[ 21 ] = "DialAreaCode=TRUE";										// XXX
	dialerData[ 22 ] = "ScriptEnabled=" + scriptEnabledFlag;
	dialerData[ 23 ] = "ScriptFileName=" + scriptFile;
	dialerData[ 24 ] = "NeedsTTYWindow=" + ttyFlag;
	dialerData[ 25 ] = "Location=Home";
 	dialerData[ 26 ] = "DisconnectTime=" + globals.GetNameValuePair( acctSetupIni, "Mode Selection", "Dialer_Disconnect_After" );
	dialerData[ 27 ] = "Path=Server";

	// * write out dialer data to Java Console
	if ( documentVars.debugMode.value.toLowerCase() == "yes" )
	{
		globals.debug( "\nDialer data (for Registration Server): " );
		var numElements = dialerData.length;
		for ( var x = 0; x < numElements; x++ )
			globals.debug( "        " + x + ": " + dialerData[ x ] );
	}

	// * configure the dialer
	plugin.DialerConfig( dialerData, true );

	// * check if we need to reboot
	if ( plugin.NeedReboot() == true )
	{
		// XXX hardcode in name of next screen???
		globals.forceReboot( "1step.htm" );		
		return;
	}

	if ( plugin.DialerConnect() == false )
	{
		plugin.DialerHangup();
		// XXX hardcode in name of next screen???
		window.location.replace( "error.htm" );							
		return;
	}
}

function loadData()
{
	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	if ( parent.controls.generateControls )
		parent.controls.generateControls();

	// * only connect to reggie if editmode is off
	if ( documentVars.editMode.value != "yes" )
	{
		var configFolder = globals.getConfigFolder( self );
		var acctSetupIni = globals.getAcctSetupFilename( self );
		var regFile = configFolder + documentVars.regServer.value;


		var regSource = globals.GetNameValuePair( acctSetupIni, "Mode Selection", "RegSource" );
		
		var localFlag = globals.GetNameValuePair( regFile, "Dial-In Configuration", "LocalMode" );
		localFlag = localFlag.toLowerCase();
		globals.debug( "localFlag:" + localFlag );
		if ( localFlag != "yes" )
		{
			globals.debug( "LocalMode==no" );
			var connectStatusFlag = plugin.IsDialerConnected();
			if ( connectStatusFlag == true )
			{
				if ( confirm( "Account Setup can't connect until you close your current connection. Close the connection now?" ) == false )
					return;
				plugin.DialerHangup();
			}

			configureDialer( configFolder, acctSetupIni, regFile );
		}
		
		regCGI = globals.GetNameValuePair( regFile, "IP", "RegCGI" );
		regRoot = globals.GetNameValuePair( regFile, "Configuration", "RegRoot" );
		metadataMode = globals.GetNameValuePair( regFile, "Configuration", "MetadataMode" );
		if ( metadataMode == "no" )
			globals.debug( "MetadataMode==no, you will not be downloading necessary metadata" );
		
		if ( regCGI == null || regCGI == "" )
		{
			alert( "Internal problem determining the Registration Server." );
			return;
		}

		if ( regRoot == null )
		{
			alert( "Internal problem determining location of Registration Server data file repository (RegRoot)." );
			return;
		}
/*
		// * write out reggie data to Java Console
		if ( documentVars.debugMode.value.toLowerCase() == "yes" )
		{
			globals.debug( "\nRegServer data: " + document.forms[ 0 ].action );
			var numElements = document.forms[ 0 ].length;
			for ( var x = 0; x < document.forms[ 0 ].length; x++ )
				globals.debug( "        " + x + ": " + document.forms[ 0 ].elements[ x ].name + "=" + document.forms[ 0 ].elements[ x ].value );
		}
*/
	
		// * submit reggie data
		navigator.preference( "security.warn_submit_insecure", false );
		navigator.preference( "security.warn_entering_secure", false );
		navigator.preference( "security.warn_leaving_secure", false );

		// * automatically submit form to registration server
		//document.forms[ 0 ].submit();

		reggieData = plugin.newStringArray( 8 );		// increment this # as new dialer strings are added
		reggieData[ 0 ] = "CST_PHONE_NUMBER=" + documentVars.modemPhoneNumber.value;
		reggieData[ 1 ] = "REG_SOURCE=" + regSource;
		reggieData[ 2 ] = "CLIENT_LANGUAGE=" + navigator.language;
		reggieData[ 3 ] = "CST_AREA_CODE_1=" + documentVars.modemAreaCode.value;
		reggieData[ 4 ] = "CST_AREA_CODE_2=" + documentVars.altAreaCode1.value;
		reggieData[ 5 ] = "CST_AREA_CODE_3=" + documentVars.altAreaCode2.value;
		reggieData[ 6 ] = "CST_AREA_CODE_4=" + documentVars.altAreaCode3.value;
		reggieData[ 7 ] = "CST_COUNTRY_CODE=" + "1";
		
		/*documentVars.countryCode.value;*/
				
		//if ( localFlag != "yes" )
		//	globals.set1StepMode( 1 );
		
		var		result = plugin.GenerateComparePage( globals.getFolder( self ), regCGI, regRoot, metadataMode, reggieData );
		
		plugin.DialerHangup();
		//if ( localFlag != "yes" )
		//	globals.oneStepSemaphore = true;
		
		if ( result == true )
			window.location.replace( "compwrap.htm" );
		else
			window.location.replace( "error.htm" );
	}
}

function saveData()
{
}

// end hiding contents from old browsers  -->
