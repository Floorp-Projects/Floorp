/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Try to throw a bunch of errors.  Corresponds to 
 * the xpcTestCallJS in xpctest_calljs.cpp
 *
 */

StartTest( "Evaluate JavaScript Throw Expressions" );

SetupTest();
AddTestData();
AddTestComment();
StopTest();

function SetupTest() {
	CONTRACTID = "@mozilla.org/js/xpc/test/CallJS;1";
	CLASS = Components.classes[CONTRACTID].createInstance();
	IFACE = Components.interfaces.nsIXPCTestCallJS;
	ERROR = Components.results;

	caller = CLASS.QueryInterface(IFACE);

	JSO = new Function();
	JSO.prototype.Evaluate = new Function( "s", 
		"this.result = eval(s); return this.result" );
	JSO.prototype.gotException = false;
	JSO.prototype.exception = {};
	JSO.prototype.EvaluateAndReturnError = new Function( "s", "r", "r = eval(s); return r;" );

	// keep track of the errors we've tried to use.

	ERRORS_WE_GOT = {};

	for ( var p in Components.results ) {
		ERRORS_WE_GOT[p] = false;
	}
}

function AddTestData() {
	var jso2 = new JSO();
	jso2.Evaluate = new Function( "return NS_ERROR_XPC_JS_THREW_EXCEPTION" );

	TestEvaluate(
		"3",
		undefined,
		true,
		"NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS",
		jso2 );

	/*
	 *	Throw args based on the number of args
	 */

	TestEvaluateArgs(
		[],
		undefined,
		true,
		"NS_ERROR_XPC_NOT_ENOUGH_ARGS" );

	TestEvaluateArgs(
		[3,2],
		3,
		false);

	/*
	 *	Set the JSOobject to something that is not an instance of nsIXPCTestCallJS
	 *
	 */

	TestSetJSObject(
		true,
		undefined,
		true,
		"NS_ERROR_XPC_BAD_CONVERT_JS");

}

function TestEvaluate (evalArg, eResult, eGotException, eExceptionName, jso ) {
	if ( !jso )
		var jso = new JSO();

	caller.SetJSObject(jso);
	
	try {
		caller.Evaluate(evalArg);
	} catch (e) {
		jso.gotException = true;
		jso.exception = e;
	} finally {
		AddTestCase(
			"caller.Evaluate(" +evalArg+ "); jso.result ",
			eResult,
			jso.result);

		AddTestCase(
			"caller.Evaluate(" +evalArg+ "); jso.gotException ",
			eGotException,
			jso.gotException );

		AddTestCase(
			"caller.Evaluate(" +evalArg+ "); jso.exception.name",
			eExceptionName,
			jso.exception.name );

		AddTestCase(
			"caller.Evaluate(" +evalArg+ "); jso.exception.result",
			Components.results[eExceptionName],
			jso.exception.result );

		if ( jso.gotException ) {
			ERRORS_WE_GOT[ jso.exception.name ] = true;
		}
	}
}

function TestEvaluateToString (evalArg, outArg, eResult, eGotException, eExceptionName ) {
	var jso = new JSO();
	caller.SetJSObject(jso);
	
	try {
		caller.EvaluateToString(evalArg, outArg);
	} catch (e) {
		jso.gotException = true;
		jso.exception = e;
	} finally {
		AddTestCase(
			"caller.EvaluateToString(" +evalArg+","+outArg+"); jso.result ",
			eResult,
			jso.result);

		AddTestCase(
			"caller.EvaluateToString(" +evalArg+","+outArg+"); outArg ",
			eResult,
			outArg);

		AddTestCase(
			"jso.gotException ",
			eGotException,
			jso.gotException );

		AddTestCase(
			"jso.exception.name",
			eExceptionName,
			jso.exception.name );

		AddTestCase(
			"jso.exception.result",
			Components.results[eExceptionName],
			jso.exception.result );

		if ( jso.gotException ) {
			ERRORS_WE_GOT[ jso.exception.name ] = true;
		}
	}
}

function TestEvaluateArgs( argsArray, eResult, eGotException, eExceptionName ) {
	var jso = new JSO;
	caller.SetJSObject(jso);
	
	try {
		if ( argsArray.length > 0 ) {
			eval( "caller.Evaluate("+argsArray.toString()+")" );
		} else {
			caller.Evaluate();
		}
	} catch (e) {
		jso.gotException = true;
		jso.exception = e;
	} finally {
		AddTestCase(
			"callerEvaluate(" +argsArray+ "); jso.result ",
			eResult,
			jso.result);

		AddTestCase(
			"jso.gotException ",
			eGotException,
			jso.gotException );

		AddTestCase(
			"jso.exception.result",
			Components.results[eExceptionName],
			jso.exception.result );

		AddTestCase(
			"jso.exception.name",
			eExceptionName,
			jso.exception.name );

		if ( jso.gotException ) {
			ERRORS_WE_GOT[ jso.exception.name ] = true;
		}

	}
}
function TestSetJSObject( jso, eResult, eGotException, eExceptionName ) {
	var exception = {};
	var gotException = false;
	var result;
	
	try {
		result = caller.SetJSObject( jso );
	} catch (e) {
		gotException = true;	
		exception = e;
	} finally {
		AddTestCase(
			"caller.SetJSObject(" + jso +")",
			eResult,
			result );

		AddTestCase(
			"gotException? ",
			eGotException,
			gotException);

		AddTestCase(
			"exception.name",
			eExceptionName,
			exception.name );

		if ( gotException ) {
			ERRORS_WE_GOT[ exception.name ] = true;
		}
	}
}

function AddTestComment() {
	var s = "This test exercised the exceptions defined in "+
		"Components.results. The following errors were not exercised:\n";

	for ( var p in ERRORS_WE_GOT ) {
		if ( ERRORS_WE_GOT[p] == false ) {
			s += p +"\n";
		}
	}

	AddComment(s);

}

function JSException ( message, name, data ) {
	this.message = message;
	this.name = name;
	this.result = Components.results[name];
	this.location = 0;
	this.data = (data) ? data : null;
	this.initialize = new Function();
	this.toString = new Function ( "return this.message" );
}

function TryIt(s) {
	try {
		eval(s);
	} catch (e) {
		Enumerate(e);
	}
}
