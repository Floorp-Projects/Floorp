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
 * Verify that we can access but not overwrite the values of read-only 
 * attributes.
 */

 StartTest( "Read-Write Attributes" );

 /*
  * These values come from xpctest_attributes.idl and xpctest_attributes.cpp
  */

 var CONTRACTID = "@mozilla.org/js/xpc/test/ObjectReadWrite;1";
 var INAME   = Components.interfaces["nsIXPCTestObjectReadWrite"];

 var O = Components.classes[CONTRACTID].createInstance();
 o = O.QueryInterface( INAME );


 AddTestCase( "typeof Components.classes[" + CONTRACTID+"].createInstance()",
			   "object",
			   typeof O );

 AddTestCase( "typeof O.QueryInterface[" +INAME+"]",
			"object",
			typeof o );

 AddTestCase( "o.booleanProperty", true, o.booleanProperty );
 AddTestCase( "o.shortProperty", 32767, o.shortProperty );
 AddTestCase( "o.longProperty", 2147483647, o.longProperty );
 AddTestCase( "o.charProperty", "X", o.charProperty );
			  
 // these we can overwrite

 o.booleanProperty = false;
 o.shortProperty = -12345;
 o.longProperty = 1234567890;
 o.charProperty = "Z";

 AddTestCase( "o.booleanProperty", false, o.booleanProperty );
 AddTestCase( "o.shortProperty", -12345, o.shortProperty );
 AddTestCase( "o.longProperty", 1234567890, o.longProperty );
 AddTestCase( "o.charProperty", "Z", o.charProperty );

 // try assigning values that differ from the expected type to verify
 // conversion
 SetAndTestBooleanProperty( false, false );
 SetAndTestBooleanProperty( 1, true );
 SetAndTestBooleanProperty( null, false );
 SetAndTestBooleanProperty( "A", true );
 SetAndTestBooleanProperty( undefined, false );
 SetAndTestBooleanProperty( [], true );
 SetAndTestBooleanProperty( {}, true );

 StopTest();

 function SetAndTestBooleanProperty( newValue, expectedValue ) {
	 o.booleanProperty = newValue;

	 AddTestCase( "o.booleanProperty = " + newValue +"; o.booleanProperty", 
				expectedValue, 
				o.booleanProperty );
 }
