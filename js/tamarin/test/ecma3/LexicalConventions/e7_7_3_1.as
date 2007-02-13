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

var SECTION = "7.7.3-1";
var VERSION = "ECMA_1";
    startTest();
var TITLE   = "Numeric Literals";
var BUGNUMBER="122877";

writeHeaderToLog( SECTION + " "+ TITLE);

var testcases = getTestCases();

test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    
    array[item++] = new TestCase( SECTION,
        "0x12345678",
        305419896,
        0x12345678 );
    
    array[item++] = new TestCase( SECTION,
        "0x80000000",
        2147483648,
        0x80000000 );
    
    array[item++] = new TestCase( SECTION,
        "0xffffffff",
        4294967295,
        0xffffffff );
    
    array[item++] = new TestCase( SECTION,
        "0x100000000",
        4294967296,
        0x100000000 );
    
    // we will not support octal for flash 8
    /*array[item++] = new TestCase( SECTION,
        "077777777777777777",
        2251799813685247,
        077777777777777777 );
    
    array[item++] = new TestCase( SECTION,
        "077777777777777776",
        2251799813685246,
        077777777777777776 );
    */
    array[item++] = new TestCase( SECTION,
        "0x1fffffffffffff",
        9007199254740991,
        0x1fffffffffffff );
    
    array[item++] = new TestCase( SECTION,
        "0x20000000000000",
        9007199254740992,
        0x20000000000000 );
    
    array[item++] = new TestCase( SECTION,
        "0x20123456789abc",
        9027215253084860,
        0x20123456789abc );
    
    array[item++] = new TestCase( SECTION,
        "0x20123456789abd",
        9027215253084860,
        0x20123456789abd );
    
    array[item++] = new TestCase( SECTION,
        "0x20123456789abe",
        9027215253084862,
        0x20123456789abe );
    
    array[item++] = new TestCase( SECTION,
        "0x20123456789abf",
        9027215253084864,
        0x20123456789abf );
    
    array[item++] = new TestCase( SECTION,
        "0x1000000000000080",
        1152921504606847000,
        0x1000000000000080 );
    
    array[item++] = new TestCase( SECTION,
        "0x1000000000000081",
        1152921504606847200,
        0x1000000000000081 );
    
    array[item++] = new TestCase( SECTION,
        "0x1000000000000100",
        1152921504606847200,
        0x1000000000000100 );
    
    array[item++] = new TestCase( SECTION,
        "0x100000000000017f",
        1152921504606847200,
        0x100000000000017f );
    
    array[item++] = new TestCase( SECTION,
        "0x1000000000000180",
        (1152921504606847500),
        0x1000000000000180);
    
    array[item++] = new TestCase( SECTION,
        "0x1000000000000181",
        1152921504606847500,
        0x1000000000000181 );
    
    array[item++] = new TestCase( SECTION,
        "0x10000000000001f0",
        1152921504606847500,
        0x10000000000001f0 );
    
    array[item++] = new TestCase( SECTION,
        "0x1000000000000200",
        1152921504606847500,
        0x1000000000000200 );
    
    array[item++] = new TestCase( SECTION,
        "0x100000000000027f",
        1152921504606847500,
        0x100000000000027f );
    
    array[item++] = new TestCase( SECTION,
        "0x1000000000000280",
        1152921504606847500,
        0x1000000000000280 );
    
    array[item++] = new TestCase( SECTION,
        "0x1000000000000281",
        1152921504606847700,
        0x1000000000000281 );
    
    array[item++] = new TestCase( SECTION,
        "0x10000000000002ff",
        1152921504606847700,
        0x10000000000002ff );
    
    array[item++] = new TestCase( SECTION,
        "0x1000000000000300",
        1152921504606847700,
        0x1000000000000300 );
    
    array[item++] = new TestCase( SECTION,
        "0x10000000000000000",
        18446744073709552000,
        0x10000000000000000 );

    return array;
}
