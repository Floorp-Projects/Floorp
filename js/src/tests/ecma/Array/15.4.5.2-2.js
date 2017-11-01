/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.5.2-2.js
   ECMA Section:       Array.length
   Description:
   15.4.5.2 length
   The length property of this Array object is always numerically greater
   than the name of every property whose name is an array index.

   The length property has the attributes { DontEnum, DontDelete }.

   This test verifies that the Array.length property is not Read Only.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.4.5.2-2";
var TITLE   = "Array.length";

writeHeaderToLog( SECTION + " "+ TITLE);

addCase( new Array(), 0, Math.pow(2,14), Math.pow(2,14) );

addCase( new Array(), 0, 1, 1 );

addCase( new Array(Math.pow(2,12)), Math.pow(2,12), 0, 0 );
addCase( new Array(Math.pow(2,13)), Math.pow(2,13), Math.pow(2,12), Math.pow(2,12) );
addCase( new Array(Math.pow(2,12)), Math.pow(2,12), Math.pow(2,12), Math.pow(2,12) );
addCase( new Array(Math.pow(2,14)), Math.pow(2,14), Math.pow(2,12), Math.pow(2,12) )

// some tests where array is not empty
// array is populated with strings
  for ( var arg = "", i = 0; i < Math.pow(2,12); i++ ) {
    arg +=  String(i) + ( i != Math.pow(2,12)-1 ? "," : "" );

  }
//      print(i +":"+arg);

var a = eval( "new Array("+arg+")" );

addCase( a, i, i, i );
addCase( a, i, Math.pow(2,12)+i+1, Math.pow(2,12)+i+1, true );
addCase( a, Math.pow(2,12)+5, 0, 0, true );

test();

function addCase( object, old_len, set_len, new_len, checkitems ) {
  object.length = set_len;

  new TestCase( "array = new Array("+ old_len+"); array.length = " + set_len +
		"; array.length",
		new_len,
		object.length );

  if ( checkitems ) {
    // verify that items between old and newlen are all undefined
    if ( new_len < old_len ) {
      var passed = true;
      for ( var i = new_len; i < old_len; i++ ) {
	if ( object[i] != void 0 ) {
	  passed = false;
	}
      }
      new TestCase( "verify that array items have been deleted",
		    true,
		    passed );
    }
    if ( new_len > old_len ) {
      var passed = true;
      for ( var i = old_len; i < new_len; i++ ) {
	if ( object[i] != void 0 ) {
	  passed = false;
	}
      }
      new TestCase( "verify that new items are undefined",
		    true,
		    passed );
    }
  }

}

