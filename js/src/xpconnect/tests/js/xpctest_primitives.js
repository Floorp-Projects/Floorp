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

/*
* This is a test for the various nsISupports* 'Primitives' wrappers declared
* in nsISupportsPrimitives.idl. It is a data driven tests - more values can 
* be added to the table. It creates instances of the 'wrapper' objects, sets 
* a values and then does a 'Get' of the values and compares that to the 
* original. It also exercises the 'toString' methods of the wrappers.
*
* The table has plugable compare functions to allows for 'equals' methods of
* comparision and to allow for 'close enough' comparison of floats and doubles
*/


StartTest( "nsISupports Primitives" );

// prefix used by all these contractids
contractid_prefix = "@mozilla.org/";

// an iid to use to test nsISupportsID
var iface_test = Components.interfaces.nsISupports;
// the canonical number form for that iid (e.g. {xxx-xxx...})
var i_str = iface_test.number

// just a flag used to mean 'use the initial value to toString comparisons'
var same = Object;

// the table of data...

// columns are:
// 1) contractid suffix
// 2) interface name
// 3) value
// 4) string to use to compare for toString ('same' mans use original value)
// 5) function to use for comparing values (null means use default compare) 
// 6) function to use for comparing strings (null means use default compare) 


var data = [
 ["supports-id"      ,"nsISupportsID"      ,iface_test,i_str ,eqfn  ,  null],
 ["supports-string"  ,"nsISupportsString"  ,"foo"     ,same  ,null  ,  null],
 ["supports-wstring" ,"nsISupportsWString" ,"bar"     ,same  ,null  ,  null],
 ["supports-PRBool"  ,"nsISupportsPRBool"  ,true      ,same  ,null  ,  null],
 ["supports-PRBool"  ,"nsISupportsPRBool"  ,false     ,same  ,null  ,  null],
 ["supports-PRUint8" ,"nsISupportsPRUint8" ,7         ,same  ,null  ,  null],
 ["supports-PRUint16","nsISupportsPRUint16",12345     ,same  ,null  ,  null],
 ["supports-PRUint32","nsISupportsPRUint32",123456    ,same  ,null  ,  null],
 ["supports-PRUint64","nsISupportsPRUint64",1234567   ,same  ,null  ,  null],
 ["supports-PRTime"  ,"nsISupportsPRTime"  ,12345678  ,same  ,null  ,  null],
 ["supports-char"    ,"nsISupportsChar"    ,'z'       ,same  ,null  ,  null],
 ["supports-PRInt16" ,"nsISupportsPRInt16" ,-123      ,same  ,null  ,  null],
 ["supports-PRInt32" ,"nsISupportsPRInt32" ,-3456     ,same  ,null  ,  null],
 ["supports-PRInt64" ,"nsISupportsPRInt64" ,-1234566  ,same  ,null  ,  null],
 ["supports-float"   ,"nsISupportsFloat"   , 12.0001  ,same  ,fcmp  ,  fcmp],
 ["supports-double"  ,"nsISupportsDouble"  , 1.0029202,same  ,fcmp  ,  fcmp],
];

var failureCount = 0;
var exception = null;

try {
    println("\n\n starting test... \n");
    for(i = 0; i < data.length; i++) {
        var r = data[i];
        test(r[0], r[1], r[2], r[3], r[4], r[5]);
    }
} catch(e) {
    exception = e;
    failureCount++ ;
}    


if(exception)
    println("caught exception... "+exception);

// println("\n"+(failureCount == 0 ? "all tests PASSED" : ""+failureCount+" errors"));

StopTest();

// use this to see if floats are fairly close
function fcmp(v1, v2)
{
    var f1 = parseFloat(v1);
    var f2 = parseFloat(v2);
    var retval = ((f1 - f2) < 0.001) || ((f2 - f1) < 0.001);
//    if(retval) println(v1+" is close enough to "+v2);
    return retval;
}    

function eqfn(v1, v2)
{
    return v1.equals(v2);
}    

function regular_compare(v1, v2)
{
    return v1 == v2;    
}    

function test(contractid, iid, d, string_val, val_compare_fn, str_compare_fn)
{
    var test1_result;    
    var test2_result;    
    var full_contractid = contractid_prefix+contractid;
//    println("checking... "+contractid+" "+iid+ " with "+d);
    var clazz = Components.classes[full_contractid];

	
	var result1 = (clazz) ? true : false;

	AddTestCase( "verifying class " + clazz + " is valid",
				 true,
				 result1);

	if ( ! result1 ) {
		// need to add empty test cases so that the # of test cases is
		// constant
		println("failed for... "+contractid+" with "+d+" returned "+v.data);
		AddTestCase( "str_compare_fun", true, false );
		AddTestCase( "val_compare_fun", true, false );
		return;
	}

	try {
	    var v = clazz.createInstance(Components.interfaces[iid])
		v.data = d;
	
		if(!val_compare_fn)
			val_compare_fn = regular_compare;

		if(!str_compare_fn)
			str_compare_fn = regular_compare;

		AddTestCase( "compare values",
			     true,
				 val_compare_fn(d, v.data));

		if(string_val) {
			if(string_val == same)
				AddTestCase( "str_compare_fun( "+v +","+d +")", true, 
				str_compare_fn(""+v, ""+d) );
			else
				AddTestCase( "str_compare_fn(" + v +", " + string_val +")",
						true,
						str_compare_fn(""+v, string_val) );
		} else {
			test2_result = true;
		}

	} catch ( e ) {
		WriteLine ( "Ooh, caught exception: " + e );
	}

    return;
}    

function println(s)
{
    if(typeof(this.document) == "undefined")
        print(s);
    else
        dump(s+"\n");
}
