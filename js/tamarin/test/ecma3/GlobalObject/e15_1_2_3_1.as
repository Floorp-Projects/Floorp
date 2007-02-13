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

    var SECTION = "15.1.2.3-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "parseFloat(string)";
    var BUGNUMBER= "77391";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();

    test();


function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION, "parseFloat.length",     1,              parseFloat.length );

    var thisError:String = "no error";
    try
    {
        parseFloat.length = null;
    }
    catch(e:ReferenceError)
    {
        thisError = e.toString();
    }
    finally
    {
        array[item++] = new TestCase(SECTION, "parseFloat.length = null", "ReferenceError: Error #1074", referenceError(thisError));
    }

    array[item++] = new TestCase( SECTION, "delete parseFloat.length", false, delete parseFloat.length );

    //array[item++] = new TestCase( SECTION, "delete parseFloat.length; parseFloat.length",   1,      eval("delete parseFloat.length; parseFloat.length") );

    delete parseFloat.length;
    array[item++] = new TestCase( SECTION, "delete parseFloat.length; parseFloat.length",   1,       parseFloat.length );

    var MYPROPS='';
    for ( var p in parseFloat ) {
        MYPROPS += p;
    }

    array[item++] = new TestCase( SECTION, "var MYPROPS='', for ( var p in parseFloat ) { MYPROPS += p }, MYPROPS", "", MYPROPS );

    array[item++] = new TestCase( SECTION, "parseFloat()",          Number.NaN,     parseFloat() );
    array[item++] = new TestCase( SECTION, "parseFloat('')",        Number.NaN,     parseFloat('') );

    array[item++] = new TestCase( SECTION, "parseFloat(' ')",       Number.NaN,     parseFloat(' ') );
    array[item++] = new TestCase( SECTION, "parseFloat(true)",      Number.NaN,     parseFloat(true) );
    array[item++] = new TestCase( SECTION, "parseFloat(true)",      Number.NaN,     parseFloat("true") );
    array[item++] = new TestCase( SECTION, "parseFloat(false)",     Number.NaN,     parseFloat(false) );
    array[item++] = new TestCase( SECTION, "parseFloat(false)",     Number.NaN,     parseFloat("false") );
    array[item++] = new TestCase( SECTION, "parseFloat('string')",  Number.NaN,     parseFloat("string") );

    array[item++] = new TestCase( SECTION, "parseFloat('  Infinity')",    Infinity,    parseFloat("Infinity") );
    array[item++] = new TestCase( SECTION, "parseFloat('  Infinity  ')",      Infinity,    parseFloat('  Infinity  ') );
    array[item++] = new TestCase( SECTION, "parseFloat('  Infinity  ')",      Infinity,    parseFloat(" Infinity ") );
    array[item++] = new TestCase( SECTION, "parseFloat('Infinity')",    Infinity,    parseFloat("Infinity") );
    array[item++] = new TestCase( SECTION, "parseFloat(Infinity)",      Infinity,    parseFloat(Infinity) );
    array[item++] = new TestCase( SECTION, "parseFloat(Infinity)",      Infinity,    parseFloat("Infinity") );


    array[item++] = new TestCase( SECTION, "parseFloat('  +Infinity')",         +Infinity,    parseFloat("+Infinity") );
    array[item++] = new TestCase( SECTION, "parseFloat('  -Infinity  ')",      -Infinity,    parseFloat('  -Infinity  ') );
    array[item++] = new TestCase( SECTION, "parseFloat('  -Infinity  ')",      -Infinity,    parseFloat("-Infinity") );
    array[item++] = new TestCase( SECTION, "parseFloat('+Infinity')",    +Infinity,    parseFloat("+Infinity") );
    array[item++] = new TestCase( SECTION, "parseFloat(-Infinity)",      -Infinity,    parseFloat(-Infinity) );
    array[item++] = new TestCase( SECTION, "parseFloat(-Infinity)",      -Infinity,    parseFloat("-Infinity") );
    array[item++] = new TestCase( SECTION,  "parseFloat('0')",          0,          parseFloat("0") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-0')",         -0,         parseFloat("-0") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+0')",          0,         parseFloat("+0") );

    array[item++] = new TestCase( SECTION,  "parseFloat('1')",          1,          parseFloat("1") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-1')",         -1,         parseFloat("-1") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+1')",          1,         parseFloat("+1") );

    array[item++] = new TestCase( SECTION,  "parseFloat('2')",          2,          parseFloat("2") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-2')",         -2,         parseFloat("-2") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+2')",          2,         parseFloat("+2") );

    array[item++] = new TestCase( SECTION,  "parseFloat('3')",          3,          parseFloat("3") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-3')",         -3,         parseFloat("-3") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+3')",          3,         parseFloat("+3") );

    array[item++] = new TestCase( SECTION,  "parseFloat('4')",          4,          parseFloat("4") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-4')",         -4,         parseFloat("-4") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+4')",          4,         parseFloat("+4") );

    array[item++] = new TestCase( SECTION,  "parseFloat('5')",          5,          parseFloat("5") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-5')",         -5,         parseFloat("-5") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+5')",          5,         parseFloat("+5") );

    array[item++] = new TestCase( SECTION,  "parseFloat('6')",          6,          parseFloat("6") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-6')",         -6,         parseFloat("-6") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+6')",          6,         parseFloat("+6") );

    array[item++] = new TestCase( SECTION,  "parseFloat('7')",          7,          parseFloat("7") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-7')",         -7,         parseFloat("-7") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+7')",          7,         parseFloat("+7") );

    array[item++] = new TestCase( SECTION,  "parseFloat('8')",          8,          parseFloat("8") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-8')",         -8,         parseFloat("-8") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+8')",          8,         parseFloat("+8") );

    array[item++] = new TestCase( SECTION,  "parseFloat('9')",          9,          parseFloat("9") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-9')",         -9,         parseFloat("-9") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+9')",          9,         parseFloat("+9") );

    array[item++] = new TestCase( SECTION,  "parseFloat('3.14159')",    3.14159,    parseFloat("3.14159") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-3.14159')",   -3.14159,   parseFloat("-3.14159") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+3.14159')",   3.14159,    parseFloat("+3.14159") );

    array[item++] = new TestCase( SECTION,  "parseFloat('3.')",         3,          parseFloat("3.") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-3.')",        -3,         parseFloat("-3.") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+3.')",        3,          parseFloat("+3.") );

    array[item++] = new TestCase( SECTION,  "parseFloat('3.e1')",       30,         parseFloat("3.e1") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-3.e1')",      -30,        parseFloat("-3.e1") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+3.e1')",      30,         parseFloat("+3.e1") );

    array[item++] = new TestCase( SECTION,  "parseFloat('3.e+1')",       30,         parseFloat("3.e+1") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-3.e+1')",      -30,        parseFloat("-3.e+1") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+3.e+1')",      30,         parseFloat("+3.e+1") );

    array[item++] = new TestCase( SECTION,  "parseFloat('3.e-1')",       .30,         parseFloat("3.e-1") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-3.e-1')",      -.30,        parseFloat("-3.e-1") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+3.e-1')",      .30,         parseFloat("+3.e-1") );

    // StrDecimalLiteral:::  .DecimalDigits ExponentPart opt

    array[item++] = new TestCase( SECTION,  "parseFloat('.00001')",     0.00001,    parseFloat(".00001") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+.00001')",    0.00001,    parseFloat("+.00001") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-0.0001')",    -0.00001,   parseFloat("-.00001") );

    array[item++] = new TestCase( SECTION,  "parseFloat('.01e2')",      1,          parseFloat(".01e2") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+.01e2')",     1,          parseFloat("+.01e2") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-.01e2')",     -1,         parseFloat("-.01e2") );

    array[item++] = new TestCase( SECTION,  "parseFloat('.01e+2')",      1,         parseFloat(".01e+2") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+.01e+2')",     1,         parseFloat("+.01e+2") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-.01e+2')",     -1,        parseFloat("-.01e+2") );

    array[item++] = new TestCase( SECTION,  "parseFloat('.01e-2')",      0.0001,    parseFloat(".01e-2") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+.01e-2')",     0.0001,    parseFloat("+.01e-2") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-.01e-2')",     -0.0001,   parseFloat("-.01e-2") );

    //  StrDecimalLiteral:::    DecimalDigits ExponentPart opt

    array[item++] = new TestCase( SECTION,  "parseFloat('1234e5')",     123400000,  parseFloat("1234e5") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+1234e5')",    123400000,  parseFloat("+1234e5") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-1234e5')",    -123400000, parseFloat("-1234e5") );

    array[item++] = new TestCase( SECTION,  "parseFloat('1234e+5')",    123400000,  parseFloat("1234e+5") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+1234e+5')",   123400000,  parseFloat("+1234e+5") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-1234e+5')",   -123400000, parseFloat("-1234e+5") );

    array[item++] = new TestCase( SECTION,  "parseFloat('1234e-5')",     0.01234,  parseFloat("1234e-5") );
    array[item++] = new TestCase( SECTION,  "parseFloat('+1234e-5')",    0.01234,  parseFloat("+1234e-5") );
    array[item++] = new TestCase( SECTION,  "parseFloat('-1234e-5')",    -0.01234, parseFloat("-1234e-5") );

/*
    array[item++] = new TestCase( SECTION,  "parseFloat(0)",          0,          parseFloat(0) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-0)",         -0,         parseFloat(-0) );

    array[item++] = new TestCase( SECTION,  "parseFloat(1)",          1,          parseFloat(1) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-1)",         -1,         parseFloat(-1) );

    array[item++] = new TestCase( SECTION,  "parseFloat(2)",          2,          parseFloat(2) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-2)",         -2,         parseFloat(-2) );

    array[item++] = new TestCase( SECTION,  "parseFloat(3)",          3,          parseFloat(3) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3)",         -3,         parseFloat(-3) );

    array[item++] = new TestCase( SECTION,  "parseFloat(4)",          4,          parseFloat(4) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-4)",         -4,         parseFloat(-4) );

    array[item++] = new TestCase( SECTION,  "parseFloat(5)",          5,          parseFloat(5) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-5)",         -5,         parseFloat(-5) );

    array[item++] = new TestCase( SECTION,  "parseFloat(6)",          6,          parseFloat(6) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-6)",         -6,         parseFloat(-6) );

    array[item++] = new TestCase( SECTION,  "parseFloat(7)",          7,          parseFloat(7) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-7)",         -7,         parseFloat(-7) );

    array[item++] = new TestCase( SECTION,  "parseFloat(8)",          8,          parseFloat(8) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-8)",         -8,         parseFloat(-8) );

    array[item++] = new TestCase( SECTION,  "parseFloat(9)",          9,          parseFloat(9) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-9)",         -9,         parseFloat(-9) );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.14159)",    3.14159,    parseFloat(3.14159) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.14159)",   -3.14159,   parseFloat(-3.14159) );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.)",         3,          parseFloat(3.) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.)",        -3,         parseFloat(-3.) );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.e1)",       30,         parseFloat(3.e1) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.e1)",      -30,        parseFloat(-3.e1) );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.e+1)",       30,         parseFloat(3.e+1) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.e+1)",      -30,        parseFloat(-3.e+1) );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.e-1)",       .30,         parseFloat(3.e-1) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.e-1)",      -.30,        parseFloat(-3.e-1) );


    array[item++] = new TestCase( SECTION,  "parseFloat(3.E1)",       30,         parseFloat(3.E1) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.E1)",      -30,        parseFloat(-3.E1) );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.E+1)",       30,         parseFloat(3.E+1) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.E+1)",      -30,        parseFloat(-3.E+1) );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.E-1)",       .30,         parseFloat(3.E-1) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.E-1)",      -.30,        parseFloat(-3.E-1) );

    // StrDecimalLiteral:::  .DecimalDigits ExponentPart opt

    array[item++] = new TestCase( SECTION,  "parseFloat(.00001)",     0.00001,    parseFloat(.00001) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-0.0001)",    -0.00001,   parseFloat(-.00001) );

    array[item++] = new TestCase( SECTION,  "parseFloat(.01e2)",      1,          parseFloat(.01e2) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-.01e2)",     -1,         parseFloat(-.01e2) );

    array[item++] = new TestCase( SECTION,  "parseFloat(.01e+2)",      1,         parseFloat(.01e+2) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-.01e+2)",     -1,        parseFloat(-.01e+2) );

    array[item++] = new TestCase( SECTION,  "parseFloat(.01e-2)",      0.0001,    parseFloat(.01e-2) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-.01e-2)",     -0.0001,   parseFloat(-.01e-2) );

    //  StrDecimalLiteral:::    DecimalDigits ExponentPart opt

    array[item++] = new TestCase( SECTION,  "parseFloat(1234e5)",     123400000,  parseFloat(1234e5) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-1234e5)",    -123400000, parseFloat(-1234e5) );

    array[item++] = new TestCase( SECTION,  "parseFloat(1234e+5)",    123400000,  parseFloat(1234e+5) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-1234e+5)",   -123400000, parseFloat(-1234e+5) );

    array[item++] = new TestCase( SECTION,  "parseFloat(1234e-5)",     0.01234,  parseFloat(1234e-5) );
    array[item++] = new TestCase( SECTION,  "parseFloat(-1234e-5)",    -0.01234, parseFloat(-1234e-5) );   */


    array[item++] = new TestCase( SECTION,  "parseFloat(0)",          0,          parseFloat("0") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-0)",         -0,         parseFloat("-0") );

    array[item++] = new TestCase( SECTION,  "parseFloat(1)",          1,          parseFloat("1") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-1)",         -1,         parseFloat("-1") );

    array[item++] = new TestCase( SECTION,  "parseFloat(2)",          2,          parseFloat("2") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-2)",         -2,         parseFloat("-2") );

    array[item++] = new TestCase( SECTION,  "parseFloat(3)",          3,          parseFloat("3") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3)",         -3,         parseFloat("-3") );

    array[item++] = new TestCase( SECTION,  "parseFloat(4)",          4,          parseFloat("4") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-4)",         -4,         parseFloat("-4") );

    array[item++] = new TestCase( SECTION,  "parseFloat(5)",          5,          parseFloat("5") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-5)",         -5,         parseFloat("-5") );

    array[item++] = new TestCase( SECTION,  "parseFloat(6)",          6,          parseFloat("6") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-6)",         -6,         parseFloat("-6") );

    array[item++] = new TestCase( SECTION,  "parseFloat(7)",          7,          parseFloat("7") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-7)",         -7,         parseFloat("-7") );

    array[item++] = new TestCase( SECTION,  "parseFloat(8)",          8,          parseFloat("8") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-8)",         -8,         parseFloat("-8") );

    array[item++] = new TestCase( SECTION,  "parseFloat(9)",          9,          parseFloat("9") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-9)",         -9,         parseFloat("-9") );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.14159)",    3.14159,    parseFloat("3.14159") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.14159)",   -3.14159,   parseFloat("-3.14159") );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.)",         3,          parseFloat("3.") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.)",        -3,         parseFloat("-3.") );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.e1)",       30,         parseFloat("3.e1") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.e1)",      -30,        parseFloat("-3.e1") );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.e+1)",       30,         parseFloat("3.e+1") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.e+1)",      -30,        parseFloat("-3.e+1") );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.e-1)",       .30,         parseFloat("3.e-1") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.e-1)",      -.30,        parseFloat("-3.e-1") );


    array[item++] = new TestCase( SECTION,  "parseFloat(3.E1)",       30,         parseFloat("3.E1") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.E1)",      -30,        parseFloat("-3.E1") );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.E+1)",       30,         parseFloat("3.E+1") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.E+1)",      -30,        parseFloat("-3.E+1") );

    array[item++] = new TestCase( SECTION,  "parseFloat(3.E-1)",       .30,         parseFloat("3.E-1") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-3.E-1)",      -.30,        parseFloat("-3.E-1") );

    // StrDecimalLiteral:::  .DecimalDigits ExponentPart opt

    array[item++] = new TestCase( SECTION,  "parseFloat(.00001)",     0.00001,    parseFloat(".00001") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-0.0001)",    -0.00001,   parseFloat("-.00001") );

    array[item++] = new TestCase( SECTION,  "parseFloat(.01e2)",      1,          parseFloat(".01e2") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-.01e2)",     -1,         parseFloat("-.01e2") );

    array[item++] = new TestCase( SECTION,  "parseFloat(.01e+2)",      1,         parseFloat(".01e+2") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-.01e+2)",     -1,        parseFloat("-.01e+2") );

    array[item++] = new TestCase( SECTION,  "parseFloat(.01e-2)",      0.0001,    parseFloat(".01e-2") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-.01e-2)",     -0.0001,   parseFloat("-.01e-2") );

    //  StrDecimalLiteral:::    DecimalDigits ExponentPart opt

    array[item++] = new TestCase( SECTION,  "parseFloat(1234e5)",     123400000,  parseFloat("1234e5") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-1234e5)",    -123400000, parseFloat("-1234e5") );

    array[item++] = new TestCase( SECTION,  "parseFloat(1234e+5)",    123400000,  parseFloat("1234e+5") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-1234e+5)",   -123400000, parseFloat("-1234e+5") );

    array[item++] = new TestCase( SECTION,  "parseFloat(1234e-5)",     0.01234,  parseFloat("1234e-5") );
    array[item++] = new TestCase( SECTION,  "parseFloat(-1234e-5)",    -0.01234, parseFloat("-1234e-5") );

    // hex cases should all return 0  (0 is the longest string that satisfies a StringDecimalLiteral)

    array[item++] = new TestCase( SECTION,  "parseFloat('0x0')",        0,         parseFloat("0x0"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0x1')",        0,         parseFloat("0x1"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0x2')",        0,         parseFloat("0x2"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0x3')",        0,         parseFloat("0x3"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0x4')",        0,         parseFloat("0x4"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0x5')",        0,         parseFloat("0x5"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0x6')",        0,         parseFloat("0x6"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0x7')",        0,         parseFloat("0x7"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0x8')",        0,         parseFloat("0x8"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0x9')",        0,         parseFloat("0x9"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xa')",        0,         parseFloat("0xa"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xb')",        0,         parseFloat("0xb"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xc')",        0,         parseFloat("0xc"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xd')",        0,         parseFloat("0xd"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xe')",        0,         parseFloat("0xe"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xf')",        0,         parseFloat("0xf"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xA')",        0,         parseFloat("0xA"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xB')",        0,         parseFloat("0xB"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xC')",        0,         parseFloat("0xC"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xD')",        0,         parseFloat("0xD"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xE')",        0,         parseFloat("0xE"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0xF')",        0,         parseFloat("0xF"));

    array[item++] = new TestCase( SECTION,  "parseFloat('0X0')",        0,         parseFloat("0X0"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0X1')",        0,         parseFloat("0X1"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0X2')",        0,         parseFloat("0X2"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0X3')",        0,         parseFloat("0X3"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0X4')",        0,         parseFloat("0X4"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0X5')",        0,         parseFloat("0X5"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0X6')",        0,         parseFloat("0X6"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0X7')",        0,         parseFloat("0X7"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0X8')",        0,         parseFloat("0X8"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0X9')",        0,         parseFloat("0X9"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0Xa')",        0,         parseFloat("0Xa"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0Xb')",        0,         parseFloat("0Xb"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0Xc')",        0,         parseFloat("0Xc"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0Xd')",        0,         parseFloat("0Xd"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0Xe')",        0,         parseFloat("0Xe"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0Xf')",        0,         parseFloat("0Xf"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0XA')",        0,         parseFloat("0XA"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0XB')",        0,         parseFloat("0XB"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0XC')",        0,         parseFloat("0XC"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0XD')",        0,         parseFloat("0XD"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0XE')",        0,         parseFloat("0XE"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0XF')",        0,         parseFloat("0XF"));
    array[item++] = new TestCase( SECTION,  "parseFloat('  0XF  ')",    0,         parseFloat("  0XF  "));

    // hex literals should still succeed

/*    array[item++] = new TestCase( SECTION,  "parseFloat(0x0)",        0,          parseFloat(0x0));
    array[item++] = new TestCase( SECTION,  "parseFloat(0x1)",        1,          parseFloat(0x1));
    array[item++] = new TestCase( SECTION,  "parseFloat(0x2)",        2,          parseFloat(0x2));
    array[item++] = new TestCase( SECTION,  "parseFloat(0x3)",        3,          parseFloat(0x3));
    array[item++] = new TestCase( SECTION,  "parseFloat(0x4)",        4,          parseFloat(0x4));
    array[item++] = new TestCase( SECTION,  "parseFloat(0x5)",        5,          parseFloat(0x5));
    array[item++] = new TestCase( SECTION,  "parseFloat(0x6)",        6,          parseFloat(0x6));
    array[item++] = new TestCase( SECTION,  "parseFloat(0x7)",        7,          parseFloat(0x7));
    array[item++] = new TestCase( SECTION,  "parseFloat(0x8)",        8,          parseFloat(0x8));
    array[item++] = new TestCase( SECTION,  "parseFloat(0x9)",        9,          parseFloat(0x9));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xa)",        10,         parseFloat(0xa));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xb)",        11,         parseFloat(0xb));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xc)",        12,         parseFloat(0xc));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xd)",        13,         parseFloat(0xd));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xe)",        14,         parseFloat(0xe));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xf)",        15,         parseFloat(0xf));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xA)",        10,         parseFloat(0xA));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xB)",        11,         parseFloat(0xB));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xC)",        12,         parseFloat(0xC));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xD)",        13,         parseFloat(0xD));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xE)",        14,         parseFloat(0xE));
    array[item++] = new TestCase( SECTION,  "parseFloat(0xF)",        15,         parseFloat(0xF));

    array[item++] = new TestCase( SECTION,  "parseFloat(0X0)",        0,          parseFloat(0X0));
    array[item++] = new TestCase( SECTION,  "parseFloat(0X1)",        1,          parseFloat(0X1));
    array[item++] = new TestCase( SECTION,  "parseFloat(0X2)",        2,          parseFloat(0X2));
    array[item++] = new TestCase( SECTION,  "parseFloat(0X3)",        3,          parseFloat(0X3));
    array[item++] = new TestCase( SECTION,  "parseFloat(0X4)",        4,          parseFloat(0X4));
    array[item++] = new TestCase( SECTION,  "parseFloat(0X5)",        5,          parseFloat(0X5));
    array[item++] = new TestCase( SECTION,  "parseFloat(0X6)",        6,          parseFloat(0X6));
    array[item++] = new TestCase( SECTION,  "parseFloat(0X7)",        7,          parseFloat(0X7));
    array[item++] = new TestCase( SECTION,  "parseFloat(0X8)",        8,          parseFloat(0X8));
    array[item++] = new TestCase( SECTION,  "parseFloat(0X9)",        9,          parseFloat(0X9));
    array[item++] = new TestCase( SECTION,  "parseFloat(0Xa)",        10,         parseFloat(0Xa));
    array[item++] = new TestCase( SECTION,  "parseFloat(0Xb)",        11,         parseFloat(0Xb));
    array[item++] = new TestCase( SECTION,  "parseFloat(0Xc)",        12,         parseFloat(0Xc));
    array[item++] = new TestCase( SECTION,  "parseFloat(0Xd)",        13,         parseFloat(0Xd));
    array[item++] = new TestCase( SECTION,  "parseFloat(0Xe)",        14,         parseFloat(0Xe));
    array[item++] = new TestCase( SECTION,  "parseFloat(0Xf)",        15,         parseFloat(0Xf));
    array[item++] = new TestCase( SECTION,  "parseFloat(0XA)",        10,         parseFloat(0XA));
    array[item++] = new TestCase( SECTION,  "parseFloat(0XB)",        11,         parseFloat(0XB));
    array[item++] = new TestCase( SECTION,  "parseFloat(0XC)",        12,         parseFloat(0XC));
    array[item++] = new TestCase( SECTION,  "parseFloat(0XD)",        13,         parseFloat(0XD));
    array[item++] = new TestCase( SECTION,  "parseFloat(0XE)",        14,         parseFloat(0XE));
    array[item++] = new TestCase( SECTION,  "parseFloat(0XF)",        15,         parseFloat(0XF));


    // A StringNumericLiteral may not use octal notation

    array[item++] = new TestCase( SECTION,  "parseFloat('00')",        0,         parseFloat("00"));
    array[item++] = new TestCase( SECTION,  "parseFloat('01')",        1,         parseFloat("01"));
    array[item++] = new TestCase( SECTION,  "parseFloat('02')",        2,         parseFloat("02"));
    array[item++] = new TestCase( SECTION,  "parseFloat('03')",        3,         parseFloat("03"));
    array[item++] = new TestCase( SECTION,  "parseFloat('04')",        4,         parseFloat("04"));
    array[item++] = new TestCase( SECTION,  "parseFloat('05')",        5,         parseFloat("05"));
    array[item++] = new TestCase( SECTION,  "parseFloat('06')",        6,         parseFloat("06"));
    array[item++] = new TestCase( SECTION,  "parseFloat('07')",        7,         parseFloat("07"));
    array[item++] = new TestCase( SECTION,  "parseFloat('010')",       10,        parseFloat("010"));
    array[item++] = new TestCase( SECTION,  "parseFloat('011')",       11,        parseFloat("011"));

    // A StringNumericLIteral may have any number of leading 0 digits

    array[item++] = new TestCase( SECTION,  "parseFloat('001')",        1,         parseFloat("001"));
    array[item++] = new TestCase( SECTION,  "parseFloat('0001')",       1,         parseFloat("0001"));
    array[item++] = new TestCase( SECTION,  "parseFloat('  0001  ')",       1,         parseFloat("  0001  "));

    // an octal numeric literal should be treated as an octal

    array[item++] = new TestCase( SECTION,  "parseFloat(00)",        0,         parseFloat(00));
    array[item++] = new TestCase( SECTION,  "parseFloat(01)",        1,         parseFloat(01));
    array[item++] = new TestCase( SECTION,  "parseFloat(02)",        2,         parseFloat(02));
    array[item++] = new TestCase( SECTION,  "parseFloat(03)",        3,         parseFloat(03));
    array[item++] = new TestCase( SECTION,  "parseFloat(04)",        4,         parseFloat(04));
    array[item++] = new TestCase( SECTION,  "parseFloat(05)",        5,         parseFloat(05));
    array[item++] = new TestCase( SECTION,  "parseFloat(06)",        6,         parseFloat(06));
    array[item++] = new TestCase( SECTION,  "parseFloat(07)",        7,         parseFloat(07));
    array[item++] = new TestCase( SECTION,  "parseFloat(010)",       8,        parseFloat(010));
    array[item++] = new TestCase( SECTION,  "parseFloat(011)",       9,        parseFloat(011));

    // A StringNumericLIteral may have any number of leading 0 digits

    array[item++] = new TestCase( SECTION,  "parseFloat(001)",        1,         parseFloat(001));
    array[item++] = new TestCase( SECTION,  "parseFloat(0001)",       1,         parseFloat(0001));

    // make sure it's reflexive
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.PI)",      Math.PI,        parseFloat(Math.PI));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.LN2)",     Math.LN2,       parseFloat(Math.LN2));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.LN10)",    Math.LN10,      parseFloat(Math.LN10));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.LOG2E)",   Math.LOG2E,     parseFloat(Math.LOG2E));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.LOG10E)",  Math.LOG10E,    parseFloat(Math.LOG10E));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.SQRT2)",   Math.SQRT2,     parseFloat(Math.SQRT2));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.SQRT1_2)", Math.SQRT1_2,   parseFloat(Math.SQRT1_2));

    array[item++] = new TestCase( SECTION,  "parseFloat(Math.PI+'')",      Math.PI,        parseFloat(Math.PI+''));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.LN2+'')",     Math.LN2,       parseFloat(Math.LN2+''));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.LN10+'')",    Math.LN10,      parseFloat(Math.LN10+''));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.LOG2E+'')",   Math.LOG2E,     parseFloat(Math.LOG2E+''));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.LOG10E+'')",  Math.LOG10E,    parseFloat(Math.LOG10E+''));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.SQRT2+'')",   Math.SQRT2,     parseFloat(Math.SQRT2+''));
    array[item++] = new TestCase( SECTION,  "parseFloat(Math.SQRT1_2+'')", Math.SQRT1_2,   parseFloat(Math.SQRT1_2+''));

*/    return ( array );
}
