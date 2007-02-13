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
    var SECTION = "15.5.4.5-2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.charCodeAt";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var TEST_STRING = new String( " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~" );

    var testcases = getTestCases();

    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

	
    var x;

    var origBooleanCharCodeAt = Boolean.prototype.charCodeAt;
	Boolean.prototype.charCodeAt=String.prototype.charCodeAt;

    array[item++] = new TestCase( SECTION,     
			"x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(0)", 
			0x0074,    
			(x = new Boolean(true), x.charCodeAt(0)) );
    array[item++] = new TestCase( SECTION,     
			"x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(1)", 
			0x0072,    
			(x = new Boolean(true), x.charCodeAt(1)) );
    array[item++] = new TestCase( SECTION,     
			"x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(2)", 
			0x0075,    
			(x = new Boolean(true), x.charCodeAt(2)) );
    array[item++] = new TestCase( SECTION,     
			"x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(3)", 
			0x0065,    
			(x = new Boolean(true), x.charCodeAt(3)) );
    array[item++] = new TestCase( SECTION,     
			"x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(4)", 
			Number.NaN,     
			(x = new Boolean(true), x.charCodeAt(4)) );
    array[item++] = new TestCase( SECTION,     
			"x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(-1)", 
			Number.NaN,    
			(x = new Boolean(true), x.charCodeAt(-1)) );

    array[item++] = new TestCase( SECTION,     
			"x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(true)",  
			0x0072,    
			(x = new Boolean(true), x.charCodeAt(true)) );
    array[item++] = new TestCase( SECTION,     
			"x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(false)", 
			0x0074,    
			(x = new Boolean(true), x.charCodeAt(false)) );

    array[item++] = new TestCase( SECTION,     "x = new String(); x.charCodeAt(0)",    Number.NaN,     (x=new String(),x.charCodeAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = new String(); x.charCodeAt(1)",    Number.NaN,     (x=new String(),x.charCodeAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = new String(); x.charCodeAt(-1)",   Number.NaN,     (x=new String(),x.charCodeAt(-1)) );

    array[item++] = new TestCase( SECTION,     "x = new String(); x.charCodeAt(NaN)",                       Number.NaN,     (x=new String(),x.charCodeAt(Number.NaN)) );
    array[item++] = new TestCase( SECTION,     "x = new String(); x.charCodeAt(Number.POSITIVE_INFINITY)",  Number.NaN,     (x=new String(),x.charCodeAt(Number.POSITIVE_INFINITY)) );
    array[item++] = new TestCase( SECTION,     "x = new String(); x.charCodeAt(Number.NEGATIVE_INFINITY)",  Number.NaN,     (x=new String(),x.charCodeAt(Number.NEGATIVE_INFINITY)) );

    array[item++] = new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(0)",    0x0031,   (x = new Array(1,2,3), x.charCodeAt = String.prototype.charCodeAt, x.charCodeAt(0)) );
    array[item++] = new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(1)",    0x002C,   (x = new Array(1,2,3), x.charCodeAt = String.prototype.charCodeAt, x.charCodeAt(1)) );
    array[item++] = new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(2)",    0x0032,   (x = new Array(1,2,3), x.charCodeAt = String.prototype.charCodeAt, x.charCodeAt(2)) );
    array[item++] = new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(3)",    0x002C,   (x = new Array(1,2,3), x.charCodeAt = String.prototype.charCodeAt, x.charCodeAt(3)) );
    array[item++] = new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(4)",    0x0033,   (x = new Array(1,2,3), x.charCodeAt = String.prototype.charCodeAt, x.charCodeAt(4)) );
    array[item++] = new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(5)",    NaN,   (x = new Array(1,2,3), x.charCodeAt = String.prototype.charCodeAt, x.charCodeAt(5)) );

    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(0)", 0x005B, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(0)) );
    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(1)", 0x006F, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(1)) );
    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(2)", 0x0062, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(2)) );
    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(3)", 0x006A, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(3)) );
    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(4)", 0x0065, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(4)) );
    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(5)", 0x0063, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(5)) );
    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(6)", 0x0074, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(6)) );

    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(7)", 0x0020, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(7)) );

    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(8)", 0x004F, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(8)) );
    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(9)", 0x0062, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(9)) );
    array[item++] = new TestCase( SECTION,  "x = function() { this.charCodeAt = String.prototype.charCodeAt }; f = new x(); f.charCodeAt(10)", 0x006A, (x = function() { this.charCodeAt = String.prototype.charCodeAt; }, f = new x(), f.charCodeAt(10)) );

    //restore
	Boolean.prototype.charCodeAt = origBooleanCharCodeAt;
	
    return (array );
}

