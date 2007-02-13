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
    var SECTION = "15.6.2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "15.6.2 The Boolean Constructor; 15.6.2.1 new Boolean( value ); 15.6.2.2 new Boolean()";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();

    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,   "typeof (new Boolean(1))",         "boolean",            typeof (new Boolean(1)) );
    array[item++] = new TestCase( SECTION,   "(new Boolean(1)).constructor",    Boolean.prototype.constructor,   (new Boolean(1)).constructor );

    var TESTBOOL:Boolean=new Boolean(1);
    array[item++] = new TestCase( SECTION,"TESTBOOL.toString()","true",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean(1)).valueOf()",true,         (new Boolean(1)).valueOf() );
    array[item++] = new TestCase( SECTION,"typeof new Boolean(1)","boolean",    typeof (new Boolean(1)) );
    array[item++] = new TestCase( SECTION,   "(new Boolean(0)).constructor",    Boolean.prototype.constructor,   (new Boolean(0)).constructor );

	var TESTBOOL:Boolean=new Boolean(0);
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()","false",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean(0)).valueOf()",   false,       (new Boolean(0)).valueOf() );
	array[item++] = new TestCase( SECTION,   "typeof new Boolean(0)","boolean",typeof (new Boolean(0)) );
	array[item++] = new TestCase( SECTION,   "(new Boolean(-1)).constructor",    Boolean.prototype.constructor,   (new Boolean(-1)).constructor );

 	var TESTBOOL:Boolean=new Boolean(-1);
	array[item++] = new TestCase( SECTION,"TESTBOOL.toString()","true",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean(-1)).valueOf()",   true,       (new Boolean(-1)).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean(-1)",         "boolean",   typeof (new Boolean(-1)) );
    array[item++] = new TestCase( SECTION,   "(new Boolean('')).constructor",    Boolean.prototype.constructor,   (new Boolean('')).constructor );

    var TESTBOOL:Boolean=new Boolean("");
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()", "false",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean('')).valueOf()",   false,       (new Boolean("")).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean('')",         "boolean",   typeof (new Boolean("")) );
    array[item++] = new TestCase( SECTION,   "(new Boolean('1')).constructor",    Boolean.prototype.constructor,   (new Boolean("1")).constructor );

    var TESTBOOL:Boolean=new Boolean('1');
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()", "true",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean('1')).valueOf()",   true,       (new Boolean('1')).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean('1')",         "boolean",   typeof (new Boolean('1')) );
    array[item++] = new TestCase( SECTION,   "(new Boolean('0')).constructor",    Boolean.prototype.constructor,   (new Boolean('0')).constructor );

    var TESTBOOL:Boolean=new Boolean('0');
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()", "true",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean('0')).valueOf()",   true,       (new Boolean('0')).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean('0')",         "boolean",   typeof (new Boolean('0')) );
    array[item++] = new TestCase( SECTION,   "(new Boolean('-1')).constructor",    Boolean.prototype.constructor,   (new Boolean('-1')).constructor );

    var TESTBOOL:Boolean=new Boolean('-1');
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()","true",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean('-1')).valueOf()",   true,       (new Boolean('-1')).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean('-1')",         "boolean",   typeof (new Boolean('-1')) );
    array[item++] = new TestCase( SECTION,   "(new Boolean(new Boolean(true))).constructor",    Boolean.prototype.constructor,   (new Boolean(new Boolean(true))).constructor );

    var TESTBOOL:Boolean=new Boolean(new Boolean(true));
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()", "true",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean(new Boolean(true))).valueOf()",   true,       (new Boolean(new Boolean(true))).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean(new Boolean(true))",         "boolean",   typeof (new Boolean(new Boolean(true))) );
    array[item++] = new TestCase( SECTION,   "(new Boolean(Number.NaN)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.NaN)).constructor );

   	var TESTBOOL:Boolean=new Boolean(Number.NaN);
	array[item++] = new TestCase( SECTION,"TESTBOOL.toString()","false",TESTBOOL.toString());
   	array[item++] = new TestCase( SECTION,   "(new Boolean(Number.NaN)).valueOf()",   false,       (new Boolean(Number.NaN)).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean(Number.NaN)",         "boolean",   typeof (new Boolean(Number.NaN)) );
    array[item++] = new TestCase( SECTION,   "(new Boolean(null)).constructor",    Boolean.prototype.constructor,   (new Boolean(null)).constructor );

    var TESTBOOL:Boolean=new Boolean(null);
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()","false",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean(null)).valueOf()",   false,       (new Boolean(null)).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean(null)",         "boolean",   typeof (new Boolean(null)) );
    array[item++] = new TestCase( SECTION,   "(new Boolean(void 0)).constructor",    Boolean.prototype.constructor,   (new Boolean(void 0)).constructor );

	var TESTBOOL:Boolean=new Boolean(void 0);
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()","false",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean(void 0)).valueOf()",   false,       (new Boolean(void 0)).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean(void 0)",         "boolean",   typeof (new Boolean(void 0)) );
    array[item++] = new TestCase( SECTION,   "(new Boolean(Number.POSITIVE_INFINITY)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.POSITIVE_INFINITY)).constructor );

    var TESTBOOL:Boolean=new Boolean(Number.POSITIVE_INFINITY);
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()", "true",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean(Number.POSITIVE_INFINITY)).valueOf()",   true,       (new Boolean(Number.POSITIVE_INFINITY)).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean(Number.POSITIVE_INFINITY)",         "boolean",   typeof (new Boolean(Number.POSITIVE_INFINITY)) );
    array[item++] = new TestCase( SECTION,   "(new Boolean(Number.NEGATIVE_INFINITY)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.NEGATIVE_INFINITY)).constructor );

    var TESTBOOL:Boolean=new Boolean(Number.NEGATIVE_INFINITY);
	array[item++] = new TestCase( SECTION, "TESTBOOL.toString()", "true",TESTBOOL.toString());
    array[item++] = new TestCase( SECTION,   "(new Boolean(Number.NEGATIVE_INFINITY)).valueOf()",   true,       (new Boolean(Number.NEGATIVE_INFINITY)).valueOf() );
    array[item++] = new TestCase( SECTION,   "typeof new Boolean(Number.NEGATIVE_INFINITY)",         "boolean",   typeof (new Boolean(Number.NEGATIVE_INFINITY) ));
    array[item++] = new TestCase( SECTION,   "(new Boolean(Number.NEGATIVE_INFINITY)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.NEGATIVE_INFINITY)).constructor );

    var TESTBOOL:Boolean=new Boolean();
	array[item++] = new TestCase( "15.6.2.2", "TESTBOOL.toString()","false",TESTBOOL.toString());
    array[item++] = new TestCase( "15.6.2.2",   "(new Boolean()).valueOf()",   false,       (new Boolean()).valueOf() );
    array[item++] = new TestCase( "15.6.2.2",   "typeof new Boolean()",        "boolean",    typeof (new Boolean()) );

    return ( array );
}
