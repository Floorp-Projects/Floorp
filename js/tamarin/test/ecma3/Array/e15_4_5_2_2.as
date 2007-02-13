/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

    var SECTION = "15.4.5.2-2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Array.length";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = new Array();

    addCase( new Array(), 0, Math.pow(2,14), Math.pow(2,14) );

    addCase( new Array(), 0, 1, 1 );

    addCase( new Array(Math.pow(2,12)), Math.pow(2,12), 0, 0 );
    addCase( new Array(Math.pow(2,13)), Math.pow(2,13), Math.pow(2,12), Math.pow(2,12) );
    addCase( new Array(Math.pow(2,12)), Math.pow(2,12), Math.pow(2,12), Math.pow(2,12) );
    addCase( new Array(Math.pow(2,14)), Math.pow(2,14), Math.pow(2,12), Math.pow(2,12) )

    // some tests where array is not empty
    // array is populated with strings
    var length = Math.pow(2,12);
    var a = new Array(length);
    for (var i = 0; i < Math.pow(2,12); i++ ) {
		a[i] = i;
    }
    addCase( a, i, i, i );
    addCase( a, i, Math.pow(2,12)+i+1, Math.pow(2,12)+i+1, true );
    addCase( a, Math.pow(2,12)+5, 0, 0, true );

    test();

function addCase( object, old_len, set_len, new_len, ... rest) {
	var checkitems;
	if( rest.length == 1 ){
		checkitems = rest[0];
	}

    object.length = set_len;

    testcases[testcases.length] = new TestCase( SECTION,
        "array = new Array("+ old_len+"); array.length = " + set_len +
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
        testcases[testcases.length] = new TestCase( SECTION,
            "verify that array items have been deleted",
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
        testcases[testcases.length] = new TestCase( SECTION,
            "verify that new items are undefined",
            true,
            passed );
    }
    }

}
