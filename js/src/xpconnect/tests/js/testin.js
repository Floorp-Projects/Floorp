/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

 /**
  * Test the in keyword.  See xpctest_in.cpp and xpctest_in.idl for
  * function names and parameters.
  */

  StartTest( "Passing different types in" );

  var CLASS = Components.classes["@mozilla.org/js/xpc/test/In;1"];
  var IFACE = Components.interfaces["nsIXPCTestIn" ];

  var C = CLASS.createInstance();
  var c = C.QueryInterface(IFACE);

  // different tests for each type.  it would be easier to generate
  // tests using eval. maybe later.

  TestLong( 
	[0, 0,   true, 1,    "A", 0,    new Boolean, 0,   {}, 0,
	 [],0,   new Date(987654321),987654321,  new Number(2134), 2134,
	 -987654321, -987654321,  undefined, 0,   null, 0,  void 0, 0,
	 NaN, 0, Function, 0  ]
  );

  TestLong(
	[0.2134, 0,   -0.2134, 0,     Math.pow(2,16)-1, Math.pow(2,16)-1,
	Math.pow(2,31)-1, Math.pow(2,31)-1,    Math.pow(2,31), -Math.pow(2,31),
	-(Math.pow(2,31)),   (-Math.pow(2,31)),  
	(-Math.pow(2,31))+1, (-Math.pow(2,31))+1,
	(-Math.pow(2,31))-1, (Math.pow(2,31))-1
	] );

  TestShort( [
    new Number(Math.pow(2,15)-1), Math.pow(2,15)-1,
    Math.pow(2,15)-1, Math.pow(2,15)-1,
	Math.pow(2,15), -Math.pow(2,15)
  ]);

  TestChar( [
	"A", "A",
	1,	 "1",
	255, "2",
	"XPConnect", "X",
	String.fromCharCode(Math.pow(2,16)), String.fromCharCode(Math.pow(2,16))
  ]);

  TestBoolean( [
  ]);
  TestOctet( [
  ]);
  TestLongLong( [
  ]);
  TestUnsignedShort( [
  ]);
  TestUnsignedLong( [
  ]);
  TestFloat( [
  ]);
  TestDouble( [
  ]);
  TestWchar([
  ]);
  TestString([
  ]);
  TestPRBool( [
  ]);
  TestPRInt32( [
  ]);
  TestPRInt16( [
  ]);
  TestPrInt64( [
  ]);
  TestPRUint8( [
    new Number(Math.pow(2,8)), 0,
    Math.pow(2,8)-1, Math.pow(2,8)-1,
	Math.pow(2,8), 0,
	-Math.pow(2,8), 0,

  ]);
  TestPRUint16( [
  ]);
  TestPRUint32( [
  ]);
  TestPRUint64( [
  ]);

  TestVoidStar( [
  ]);

  TestCharStar( [
  ]);

  StopTest();

  function TestLong( data ) {
    for ( var i = 0; i < data.length; i+=2 ) {
		AddTestCase( "c.EchoLong("+data[i]+")",
			data[i+1],
			c.EchoLong(data[i]));
	}
  }
  function TestShort( data ) {
    for ( var i = 0; i < data.length; i+=2 ) {
		AddTestCase( "c.EchoShort("+data[i]+")",
			data[i+1],
			c.EchoShort(data[i]));
	}
  }
  function TestChar( data ) {
    for ( var i = 0; i < data.length; i+=2 ) {
		AddTestCase( "c.EchoChar("+data[i]+")",
			data[i+1],
			c.EchoChar(data[i]));
	}
  }
  function TestBoolean() {
  }
  
  function TestOctet() {
  }
  function TestLongLong() {
  }
  function TestUnsignedShort() {
  }
  function TestUnsignedLong() {
  }
  function TestFloat() {
  }
  function TestDouble() {
  }
  function TestWchar() {
  }
  function TestString() {
  }
  function TestPRBool() {
  }
  function TestPRInt32() {
  }
  function TestPRInt16() {
  }
  function TestPrInt64() {
  }
  function TestPRUint16() {
  }
  function TestPRUint32() {
  }
  function TestPRUint64() {
  }
  function TestPRUint8( data ) {
    for ( var i = 0; i < data.length; i+=2 ) {
		AddTestCase( "c.EchoPRUint8("+data[i]+")",
			data[i+1],
			c.EchoPRUint8(data[i]));
	}
  }
  function TestVoidStar( data ) {
	for ( var i = 0; i < data.length; i+=2 ) {
		AddTestCase( "c.EchoVoidStar("+data[i]+")",
			data[i+1],
			c.EchoVoidStar(data[i]));
	}
  }
  function TestCharStar() {
  }

