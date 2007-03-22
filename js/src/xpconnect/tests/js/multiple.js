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
 *	Test interfaces that inherit from interfaces other than nsISupports
 *  and classes that inherit from multiple interfaces.
 *
 *  See xpctest_multiple.idl and xpctest_multiple.cpp.
 */


StartTest( "Interfaces that inherit from interfaces other than nsISupports" );
SetupTest();
AddTestData();
StopTest();

function SetupTest() {
try {		
	iChild = Components.interfaces.nsIXPCTestChild3;
	iParentOne = Components.interfaces.nsIXPCTestParentOne;

	CONTRACTID = "@mozilla.org/js/xpc/test/Child3;1";
	cChild = Components.classes[CONTRACTID].createInstance();
	CONTRACTID = "@mozilla.org/js/xpc/test/ParentOne;1";
	cParentOne = Components.classes[CONTRACTID].createInstance();

	child = cChild.QueryInterface(iChild);
	parentOne = cParentOne.QueryInterface(iParentOne);

} catch (e) {
	WriteLine(e);
	AddTestCase(
		"Setting up the test",
		"PASSED",
		"FAILED: " + e.message  +" "+ e.location +". ");
}
}

function AddTestData() {
	// child should have all the properties and methods of parentOne

	parentOneProps = {};
	for ( p in parentOne ) parentOneProps[p] = true;
	for ( p in child ) if ( parentOneProps[p] ) parentOneProps[p] = false;

	for ( p in parentOneProps ) {
		print( p );

		AddTestCase(
			"child has property " + p,
			true,
			(parentOneProps[p] ? false : true ) 
		);

		if ( p.match(/Method/) ) {
			AddTestCase(
				"child."+p+"()",
				"@mozilla.org/js/xpc/test/Child3;1 method",
				child[p]() );

		} else if (p.match(/Attribute/)) {
			AddTestCase(
				"child." +p,
				"@mozilla.org/js/xpc/test/Child3;1 attribute",
				child[p] );
		}
	}

	var result = true;
	for ( p in parentOneProps ) {
		if ( parentOneProps[p] = true )
			result == false;
	}

	AddTestCase(
		"child has parentOne properties?",
		true,
		result );

	for ( p in child ) {
		AddTestCase(
			"child has property " + p,
			true,
			(child[p] ? true : false ) 
		);

		if ( p.match(/Method/) ) {
			AddTestCase(
				"child."+p+"()",
				"@mozilla.org/js/xpc/test/Child3;1 method",
				child[p]() );

		} else if (p.match(/Attribute/)) {
			AddTestCase(
				"child." +p,
				"@mozilla.org/js/xpc/test/Child3;1 attribute",
				child[p] );
		}
	}		
}

