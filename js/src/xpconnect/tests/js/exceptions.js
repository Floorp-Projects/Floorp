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
  * Test the in keyword.  See xpctest_in.cpp and xpctest_in.idl for
  * function names and parameters.
  */

  StartTest( "Exceptions" );

var CLASS = Components.classes["@mozilla.org/js/xpc/test/Echo;1"];
var IFACE = Components.interfaces.nsIEcho;
var nativeEcho = CLASS.createInstance(IFACE);

var localEcho =  {
    SendOneString : function() {throw this.result;},
    result : 0
};

nativeEcho.SetReceiver(localEcho);

PASS = "PASSED!";
FAIL = "FAILED!";

var result = PASS;
var error =  "";

try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    error = e;
	result = FAIL;
}

AddTestCase(
	"nativeEcho.SendOneString(\"foo\");" + error,
	PASS,
	result );



localEcho.result = new Components.Exception("hi", Components.results.NS_ERROR_ABORT);
result = FAIL;
error =  "";
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    result = PASS;

}

AddTestCase(
	"localEcho.result = new Components.Exception(\"hi\")," +
	"Components.results.NS_ERROR_ABORT);"+
	"nativeEcho.SendOneString(\"foo\");" + error,
	PASS,
	result );



localEcho.result = {message : "pretending to be NS_OK", result : 0};
result = PASS;
error =  "";

try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
	error = e;
    result = FAIL;
}
AddTestCase(
	"localEcho.result = {message : \"pretending to be NS_OK\", result : 0};"+
	"nativeEcho.SendOneString(\"foo\");" + error,
	PASS,
	result );


localEcho.result = {message : "pretending to be NS_ERROR_NO_INTERFACE", 
                    result : Components.results.NS_ERROR_NO_INTERFACE};
result = PASS;
error =  "";
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
	error = e;
    result = PASS;

}
AddTestCase(
	"localEcho.result = {message : \"pretending to be NS_ERROR_NO_INTERFACE\", "+
	"result : Components.results.NS_ERROR_NO_INTERFACE};"+
	"nativeEcho.SendOneString(\"foo\");" + error,
	PASS,
	result );


localEcho.result = "just a string";
result = FAIL;
error =  "";
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    result = PASS;
	error = e;
}
AddTestCase(
	"localEcho.result = localEcho.result = \"just a string\";"+
	"nativeEcho.SendOneString(\"foo\");" + error,
	PASS,
	result );


localEcho.result = Components.results.NS_ERROR_NO_INTERFACE;
result = FAIL;
error = "";
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    result = PASS;
	error = e;
}
AddTestCase(
	"localEcho.result = Components.results.NS_ERROR_NO_INTERFACE;" +
	"nativeEcho.SendOneString(\"foo\");" + error,
	PASS,
	result );

localEcho.result = "NS_OK";
result = FAIL;
error = "";
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    result = PASS;
	error = e;
}
AddTestCase(
	"localEcho.result = \"NS_OK\"" +
	"nativeEcho.SendOneString(\"foo\");" + error,
	PASS,
	result );


localEcho.result = Components;
result = FAIL;
error = "";
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    result = PASS;
	error = e;
}

AddTestCase( 
	"localEcho.result = Components;" +
	"nativeEcho.sendOneString(\"foo\") " + error,
	PASS,
	result);

/* make this function have a JS runtime error */
localEcho.SendOneString = function(){return Components.foo.bar;};
result = FAIL;
error = "";
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    result = PASS;
	error = e;
}
AddTestCase( 
	"localEcho.SendOneString = function(){return Components.foo.bar;};" +
	"nativeEcho.sendOneString(\"foo\") " + error,
	PASS,
	result);

/* make this function have a JS compiletime error */
localEcho.SendOneString = function(){new Function("","foo ===== 1")};
result = FAIL;
error = ""
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    result = PASS;
	error = e;
}

AddTestCase(
	"localEcho.SendOneString = function(){new Function(\"\",\"foo ===== 1\")};" +
	"nativeEcho.sendOneString(\"foo\") " + error,
	PASS,
	result);


StopTest();