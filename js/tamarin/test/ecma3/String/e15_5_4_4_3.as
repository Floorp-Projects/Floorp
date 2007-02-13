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
    var SECTION = "15.5.4.4-3";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.charAt";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function MyObject (v) {
    this.value      = v;
    this.toString   = function() { return this.value +''; }
    this.valueOf    = function() { return this.value }
    this.charAt     = String.prototype.charAt;
}
function getTestCases() {
    var array = new Array();
    var item = 0;

    var foo = new MyObject('hello');


    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello'); ", "h", foo.charAt(0)  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello'); ", "e", foo.charAt(1)  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello'); ", "l", foo.charAt(2)  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello'); ", "l", foo.charAt(3)  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello'); ", "o", foo.charAt(4)  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello'); ", "",  foo.charAt(-1)  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello'); ", "",  foo.charAt(5)  );

    var boo = new MyObject(true);

    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true); ", "t", boo.charAt(0)  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true); ", "r", boo.charAt(1)  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true); ", "u", boo.charAt(2)  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true); ", "e", boo.charAt(3)  );

    var noo = new MyObject( Math.PI );

    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "3", noo.charAt(0)  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", ".", noo.charAt(1)  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "1", noo.charAt(2)  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "4", noo.charAt(3)  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "1", noo.charAt(4)  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "5", noo.charAt(5)  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "9", noo.charAt(6)  );

    return array;
}
