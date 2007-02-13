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

    var SECTION = "15.7.1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The Number Constructor Called as a Function";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    
    test();


function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase(SECTION, "Number()",                  0,          Number() );
    array[item++] = new TestCase(SECTION, "Number(void 0)",            Number.NaN,  Number(void 0) );
    array[item++] = new TestCase(SECTION, "Number(null)",              0,          Number(null) );
    array[item++] = new TestCase(SECTION, "Number()",                  0,          Number() );
    array[item++] = new TestCase(SECTION, "Number(new Number())",      0,          Number( new Number() ) );
    array[item++] = new TestCase(SECTION, "Number(0)",                 0,          Number(0) );
    array[item++] = new TestCase(SECTION, "Number(1)",                 1,          Number(1) );
    array[item++] = new TestCase(SECTION, "Number(-1)",                -1,         Number(-1) );
    array[item++] = new TestCase(SECTION, "Number(NaN)",               Number.NaN, Number( Number.NaN ) );
    array[item++] = new TestCase(SECTION, "Number('string')",          Number.NaN, Number( "string") );
    array[item++] = new TestCase(SECTION, "Number(new String())",      0,          Number( new String() ) );
    array[item++] = new TestCase(SECTION, "Number('')",                0,          Number( "" ) );
    array[item++] = new TestCase(SECTION, "Number(Infinity)",          Number.POSITIVE_INFINITY,   Number("Infinity") );

    array[item++] = new TestCase(SECTION, "Number(-Infinity)",          Number.NEGATIVE_INFINITY,   Number("-Infinity") );

    array[item++] = new TestCase(SECTION, "Number(3.141592653589793)",          Math.PI,   Number("3.141592653589793") );

    array[item++] = new TestCase(SECTION, "Number(4294967297)",          (Math.pow(2,32)+1),   Number("4294967297") );

    array[item++] = new TestCase(SECTION, "Number(1e2000)",          Infinity,   Number(1e2000) );

    array[item++] = new TestCase(SECTION, "Number(-1e2000)",     -Infinity,   Number(-1e2000) );

    array[item++] = new TestCase(SECTION, "Number(1e-2000)",          0,   Number(1e-2000) );

    array[item++] = new TestCase(SECTION, "Number(1/1e-2000)",         Infinity,   Number(1/1e-2000) );

    array[item++] = new TestCase(SECTION, "Number(true)",         1,   Number(true) );

    array[item++] = new TestCase(SECTION, "Number(false)",         0,   Number(false) );

    array[item++] = new TestCase(SECTION, "Number(new Boolean(false)",         0,   Number(new Boolean(false)) );

    array[item++] = new TestCase(SECTION, "Number(new String('Number.POSITIVE_INFINITY')",         Infinity,   Number(new String(Number.POSITIVE_INFINITY)) );

    array[item++] = new TestCase(SECTION, "Number(new Number(false)",         0,   Number(new Number(false)) );
    

    array[item++] = new TestCase(SECTION, "Number('3000000000.25')",          (3000000000.25),   Number("3000000000.25") );

    array[item++] = new TestCase(SECTION, "Number(-'3000000000.25')",          (-3000000000.25),   Number(-"3000000000.25") );

    array[item++] = new TestCase(SECTION, "Number('1.79769313486231e+308')",(Number.MAX_VALUE+""),Number("1.79769313486231e+308")+"" );
    array[item++] = new TestCase(SECTION, "Number('4.9406564584124654e-324')",(Number.MIN_VALUE+""),Number("4.9406564584124654e-324")+"" );
    array[item++] = new TestCase(SECTION, "Number(new MyObject(100))",  100,        Number(new MyObject(100)) );

   var s:String =
"0xFFFFFFFFFFFFF80000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

    array[item++] = new TestCase(SECTION, "Number(s)",parseInt(s),   Number(s) );

    array[item++] = new TestCase(SECTION, "Number(-s)",((-5.03820925841965e+263)+""),   Number(-s)+"" );

    array[item++] = new TestCase( SECTION,
                                "Number(-\"\")",
                                0,
                                Number(-"" ));

    array[item++] = new TestCase( SECTION,
                                "Number(-\" \")",
                                0,
                                Number(-" " ));
    array[item++] = new TestCase( SECTION,
                                "Number(-\"999\")",
                                -999,
                                Number(-"999") );

    array[item++] = new TestCase( SECTION,
                                "Number(-\" 999\")",
                                -999,
                                Number(-" 999") );

    array[item++] = new TestCase( SECTION,
                                "Number(-\"\\t999\")",
                                -999,
                                Number(-"\t999") );

    array[item++] = new TestCase( SECTION,
                                "Number(-\"013  \")",
                                -13,
                                Number(-"013  " ));

    array[item++] = new TestCase( SECTION,
                                "Number(-\"999\\t\")",
                                -999,
                                Number(-"999\t") );

    array[item++] = new TestCase( SECTION,
                                "Number(-\"-Infinity\")",
                                Infinity,
                                Number(-"-Infinity" ));

    array[item++] = new TestCase( SECTION,
                                "Number(-\"-infinity\")",
                                NaN,
                                Number(-"-infinity"));


    array[item++] = new TestCase( SECTION,
                                "Number(-\"+Infinity\")",
                                -Infinity,
                                Number(-"+Infinity") );

    array[item++] = new TestCase( SECTION,
                                "Number(-\"+Infiniti\")",
                                NaN,
                                Number(-"+Infiniti"));

    array[item++] = new TestCase( SECTION,
                                "Number(- -\"0x80000000\")",
                                2147483648,
                                Number(- -"0x80000000"));

    array[item++] = new TestCase( SECTION,
                                "Number(- -\"0x100000000\")",
                                4294967296,
                                Number(- -"0x100000000") );

    array[item++] = new TestCase( SECTION,
                                "Number(- \"-0x123456789abcde8\")",
                                81985529216486880,
                                Number(- "-0x123456789abcde8"));



    return ( array );
}

function MyObject( value ) {
    this.value = value;
    this.valueOf = function() { return this.value; }
}
