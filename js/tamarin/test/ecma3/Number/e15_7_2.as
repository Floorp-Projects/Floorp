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

    var SECTION = "15.7.2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The Number Constructor";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = new Array();

    testcases = getTestCases();
    
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    //  To verify that the object's prototype is the Number.prototype, check to see if the     //  object's
    //  constructor property is the same as Number.prototype.constructor.

    array[item++] = new TestCase(SECTION, "(new Number()).constructor",      Number.prototype.constructor,   (new Number()).constructor );

    array[item++] = new TestCase(SECTION, "typeof (new Number())",         "number",           typeof (new Number()) );
    array[item++] = new TestCase(SECTION,  "(new Number()).valueOf()",     0,                   (new Number()).valueOf() );

	
    NUMB = new Number();
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number();NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "0",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "(new Number(0)).constructor",     Number.prototype.constructor,    (new Number(0)).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number(0))",         "number",           typeof (new Number(0)) );
    array[item++] = new TestCase(SECTION,  "(new Number(0)).valueOf()",     0,                   (new Number(0)).valueOf() );

    NUMB = new Number(0);
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number(0);NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "0",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "(new Number(1)).constructor",     Number.prototype.constructor,    (new Number(1)).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number(1))",         "number",           typeof (new Number(1)) );
    array[item++] = new TestCase(SECTION,  "(new Number(1)).valueOf()",     1,                   (new Number(1)).valueOf() );

    NUMB = new Number(1);
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number(1);NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "1",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "(new Number(-1)).constructor",     Number.prototype.constructor,    (new Number(-1)).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number(-1))",         "number",           typeof (new Number(-1)) );
    array[item++] = new TestCase(SECTION,  "(new Number(-1)).valueOf()",     -1,                   (new Number(-1)).valueOf() );

    NUMB = new Number(-1);
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number(-1);NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "-1",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "(new Number(Number.NaN)).constructor",     Number.prototype.constructor,    (new Number(Number.NaN)).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number(Number.NaN))",         "number",           typeof (new Number(Number.NaN)) );
    array[item++] = new TestCase(SECTION,  "(new Number(Number.NaN)).valueOf()",     Number.NaN,                   (new Number(Number.NaN)).valueOf() );

    NUMB = new Number(Number.NaN);
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number(Number.NaN);NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "NaN",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "(new Number('string')).constructor",     Number.prototype.constructor,    (new Number('string')).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number('string'))",         "number",           typeof (new Number('string')) );
    array[item++] = new TestCase(SECTION,  "(new Number('string')).valueOf()",     Number.NaN,                   (new Number('string')).valueOf() );

    NUMB = new Number('string');
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number('string');NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "NaN",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "(new Number(new String())).constructor",     Number.prototype.constructor,    (new Number(new String())).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number(new String()))",         "number",           typeof (new Number(new String())) );
    array[item++] = new TestCase(SECTION,  "(new Number(new String())).valueOf()",     0,                   (new Number(new String())).valueOf() );

    NUMB = new Number(new String());
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number(new String());NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "0",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "(new Number('')).constructor",     Number.prototype.constructor,    (new Number('')).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number(''))",         "number",           typeof (new Number('')) );
    array[item++] = new TestCase(SECTION,  "(new Number('')).valueOf()",     0,                   (new Number('')).valueOf() );

    NUMB = new Number('');
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number('');NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "0",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "(new Number(Number.POSITIVE_INFINITY)).constructor",     Number.prototype.constructor,    (new Number(Number.POSITIVE_INFINITY)).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number(Number.POSITIVE_INFINITY))",         "number",           typeof (new Number(Number.POSITIVE_INFINITY)) );
    array[item++] = new TestCase(SECTION,  "(new Number(Number.POSITIVE_INFINITY)).valueOf()",     Number.POSITIVE_INFINITY,    (new Number(Number.POSITIVE_INFINITY)).valueOf() );

    NUMB = new Number(Number.POSITIVE_INFINITY);
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number(Number.POSITIVE_INFINITY);NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "Infinity",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "(new Number(Number.NEGATIVE_INFINITY)).constructor",     Number.prototype.constructor,    (new Number(Number.NEGATIVE_INFINITY)).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number(Number.NEGATIVE_INFINITY))",         "number",           typeof (new Number(Number.NEGATIVE_INFINITY)) );
    array[item++] = new TestCase(SECTION,  "(new Number(Number.NEGATIVE_INFINITY)).valueOf()",     Number.NEGATIVE_INFINITY,                   (new Number(Number.NEGATIVE_INFINITY)).valueOf() );

    NUMB = new Number(Number.NEGATIVE_INFINITY);
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number(Number.NEGATIVE_INFINITY);NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "-Infinity",
                                NUMB.toString() );


    array[item++] = new TestCase(SECTION, "(new Number()).constructor",     Number.prototype.constructor,    (new Number()).constructor );
    array[item++] = new TestCase(SECTION, "typeof (new Number())",         "number",           typeof (new Number()) );
    array[item++] = new TestCase(SECTION,  "(new Number()).valueOf()",     0,                   (new Number()).valueOf() );

    NUMB = new Number();
    
    array[item++] = new TestCase(SECTION,
                                "NUMB = new Number();NUMB.toString=Object.prototype.toString;NUMB.toString()",
                                "0",
                                NUMB.toString() );

    array[item++] = new TestCase(SECTION, "new Number()",                  0,          new Number() );
    array[item++] = new TestCase(SECTION, "new Number(void 0)",            Number.NaN,  new Number(void 0) );
    array[item++] = new TestCase(SECTION, "new Number(null)",              0,          new Number(null) );
    
    array[item++] = new TestCase(SECTION, "new Number(new Number())",      0,          Number( new Number() ) );
    array[item++] = new TestCase(SECTION, "new Number(0)",                 0,          new Number(0) );
    array[item++] = new TestCase(SECTION, "new Number(1)",                 1,          new Number(1) );
    array[item++] = new TestCase(SECTION, "new Number(-1)",                -1,         new Number(-1) );
    array[item++] = new TestCase(SECTION, "new Number(NaN)",               Number.NaN, new Number( Number.NaN ) );
    array[item++] = new TestCase(SECTION, "new Number('string')",          Number.NaN, new Number( "string") );
    array[item++] = new TestCase(SECTION, "new Number(new String())",      0,          new Number( new String() ) );
    array[item++] = new TestCase(SECTION, "new Number('')",                0,          new Number( "" ) );
    array[item++] = new TestCase(SECTION, "new Number(Infinity)",          Number.POSITIVE_INFINITY,   new Number("Infinity") );

    array[item++] = new TestCase(SECTION, "new Number(-Infinity)",          Number.NEGATIVE_INFINITY,   new Number("-Infinity") );

    array[item++] = new TestCase(SECTION, "new Number(3.141592653589793)",          Math.PI,   new Number("3.141592653589793") );

    array[item++] = new TestCase(SECTION, "new Number(4294967297)",          (Math.pow(2,32)+1),   new Number("4294967297") );

    array[item++] = new TestCase(SECTION, "new Number(1e2000)",          Infinity,   new Number(1e2000) );

    array[item++] = new TestCase(SECTION, "new Number(-1e2000)",     -Infinity,   new Number(-1e2000) );

    array[item++] = new TestCase(SECTION, "new Number(1e-2000)",          0,   new Number(1e-2000) );

    array[item++] = new TestCase(SECTION, "new Number(1/1e-2000)",         Infinity,   new Number(1/1e-2000) );

    array[item++] = new TestCase(SECTION, "new Number(true)",         1,   new Number(true) );

    array[item++] = new TestCase(SECTION, "new Number(false)",         0,   new Number(false) );

    array[item++] = new TestCase(SECTION, "new Number(new Boolean(false)",         0,   new Number(new Boolean(false)) );

    array[item++] = new TestCase(SECTION, "new Number(new String('Number.POSITIVE_INFINITY')",         Infinity,   new Number(new String(Number.POSITIVE_INFINITY)) );

    array[item++] = new TestCase(SECTION, "new Number(new Number(false)",         0,   new Number(new Number(false)) );
    

    array[item++] = new TestCase(SECTION, "new Number('3000000000.25')",          (3000000000.25),   new Number("3000000000.25") );

    array[item++] = new TestCase(SECTION, "new Number(-'3000000000.25')",          (-3000000000.25),   new Number(-"3000000000.25") );

    array[item++] = new TestCase(SECTION, "new Number('1.79769313486231e+308')",(Number.MAX_VALUE+""),new Number("1.79769313486231e+308")+"" );
    array[item++] = new TestCase(SECTION, "new Number('4.9406564584124654e-324')",(Number.MIN_VALUE+""),new Number("4.9406564584124654e-324")+"" );
    array[item++] = new TestCase(SECTION, "new Number(new MyObject(100))",  100,        new Number(new MyObject(100)) );

   var s:String =
"0xFFFFFFFFFFFFF80000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

    array[item++] = new TestCase(SECTION, "new Number(s)",parseInt(s),new Number(s) );

    array[item++] = new TestCase(SECTION, "new Number(-s)",((-5.03820925841965e+263)+""),   new Number(-s)+"" );

    array[item++] = new TestCase( SECTION,
                                "new Number(-\"\")",
                                0,
                                new Number(-"" ));

    array[item++] = new TestCase( SECTION,
                                "new Number(-\" \")",
                                0,
                                new Number(-" " ));
    array[item++] = new TestCase( SECTION,
                                "new Number(-\"999\")",
                                -999,
                                new Number(-"999") );

    array[item++] = new TestCase( SECTION,
                                "new Number(-\" 999\")",
                                -999,
                                new Number(-" 999") );

    array[item++] = new TestCase( SECTION,
                                "new Number(-\"\\t999\")",
                                -999,
                                new Number(-"\t999") );

    array[item++] = new TestCase( SECTION,
                                "new Number(-\"013  \")",
                                -13,
                                new Number(-"013  " ));

    array[item++] = new TestCase( SECTION,
                                "new Number(-\"999\\t\")",
                                -999,
                                new Number(-"999\t") );

     array[item++] = new TestCase( SECTION,
                                "new Number(-\"-Infinity\")",
                                Infinity,
                                new Number(-"-Infinity" ));

     array[item++] = new TestCase( SECTION,
                                "new Number(-\"-infinity\")",
                                NaN,
                                new Number(-"-infinity"));


     array[item++] = new TestCase( SECTION,
                                "new Number(-\"+Infinity\")",
                                -Infinity,
                                new Number(-"+Infinity") );

     array[item++] = new TestCase( SECTION,
                                "new Number(-\"+Infiniti\")",
                                NaN,
                                new Number(-"+Infiniti"));

     array[item++] = new TestCase( SECTION,
                                "new Number(- -\"0x80000000\")",
                                2147483648,
                                new Number(- -"0x80000000"));

     array[item++] = new TestCase( SECTION,
                                "new Number(- -\"0x100000000\")",
                                4294967296,
                                new Number(- -"0x100000000") );

     array[item++] = new TestCase( SECTION,
                                "new Number(- \"-0x123456789abcde8\")",
                                81985529216486880,
                                new Number(- "-0x123456789abcde8"));


    return ( array );
}
function MyObject( value ) {
    this.value = value;
    this.valueOf = function() { return this.value; }
}
