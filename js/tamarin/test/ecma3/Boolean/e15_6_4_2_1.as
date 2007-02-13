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

    var SECTION = "15.6.4.2-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Boolean.prototype.toString()"
    writeHeaderToLog( SECTION + TITLE );

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,   "new Boolean(1)",       "true",   (new Boolean(1)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(0)",       "false",  (new Boolean(0)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(-1)",      "true",   (new Boolean(-1)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean('1')",     "true",   (new Boolean("1")).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean('0')",     "true",   (new Boolean("0")).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(true)",    "true",   (new Boolean(true)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(false)",   "false",  (new Boolean(false)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean('true')",  "true",   (new Boolean('true')).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean('false')", "true",   (new Boolean('false')).toString() );

    array[item++] = new TestCase( SECTION,   "new Boolean('')",      "false",  (new Boolean('')).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(null)",    "false",  (new Boolean(null)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(void(0))", "false",  (new Boolean(void(0))).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(-Infinity)", "true", (new Boolean(Number.NEGATIVE_INFINITY)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(NaN)",     "false",  (new Boolean(Number.NaN)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean()",        "false",  (new Boolean()).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(x=1)",     "true",   (new Boolean(x=1)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(x=0)",     "false",  (new Boolean(x=0)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(x=false)", "false",  (new Boolean(x=false)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(x=true)",  "true",   (new Boolean(x=true)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(x=null)",  "false",  (new Boolean(x=null)).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(x='')",    "false",  (new Boolean(x="")).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(x=' ')",   "true",   (new Boolean(x=" ")).toString() );

    array[item++] = new TestCase( SECTION,   "new Boolean(new MyObject(true))",     "true",   (new Boolean(new MyObject(true))).toString() );
    array[item++] = new TestCase( SECTION,   "new Boolean(new MyObject(false))",    "true",   (new Boolean(new MyObject(false))).toString() );

    return ( array );
}
function MyObject( value ) {
    this.value = value;
    this.valueOf = function() { return this.value; }
    this.valueOf = function() { return this.value; }
    return this;
}
