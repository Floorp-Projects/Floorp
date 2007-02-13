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
    var SECTION = "11_13_2_4";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " Compound Assignment: +=");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    // If either operand is NaN,  result is NaN
    VAR1 = Number.NaN; VAR2=1;
    array[item++] = new TestCase( SECTION,    "VAR1 = NaN; VAR2=1; VAR1 += VAR2",       Number.NaN,  VAR1 += VAR2 );
    VAR1 = Number.NaN; VAR2=1; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = NaN; VAR2=1; VAR1 += VAR2; VAR1", Number.NaN,  VAR1 );
    VAR1 = Number.NaN; VAR2=0; 
    array[item++] = new TestCase( SECTION,    "VAR1 = NaN; VAR2=0; VAR1 += VAR2",       Number.NaN, VAR1 += VAR2 );
    VAR1 = Number.NaN; VAR2=0; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = NaN; VAR2=0; VAR1 += VAR2; VAR1", Number.NaN,  VAR1 );
    VAR1 = 0; VAR2=Number.NaN; 
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2=NaN; VAR1 += VAR2",       Number.NaN, VAR1 += VAR2 );
    VAR1 = 0; VAR2=Number.NaN; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2=NaN; VAR1 += VAR2; VAR1", Number.NaN,  VAR1 );

    // the sum of two Infinities the same sign is the infinity of that sign
    // the sum of two Infinities of opposite sign is NaN
    VAR1 = Number.POSITIVE_INFINITY; VAR2 = Number.POSITIVE_INFINITY; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = Infinity; VAR2= Infinity; VAR1 += VAR2; VAR1",   Number.POSITIVE_INFINITY,         VAR1 );
    VAR1 = Number.POSITIVE_INFINITY; VAR2 = Number.NEGATIVE_INFINITY; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = Infinity; VAR2= -Infinity; VAR1 += VAR2; VAR1",  Number.NaN,                       VAR1 );
    VAR1 = Number.NEGATIVE_INFINITY; VAR2 = Number.POSITIVE_INFINITY; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 =-Infinity; VAR2= Infinity; VAR1 += VAR2; VAR1",   Number.NaN,                       VAR1 );
    VAR1 = Number.NEGATIVE_INFINITY; VAR2 = Number.NEGATIVE_INFINITY; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 =-Infinity; VAR2=-Infinity; VAR1 += VAR2; VAR1",   Number.NEGATIVE_INFINITY,         VAR1 );

    // the sum of an infinity and a finite value is equal to the infinite operand
    VAR1 = 0; VAR2 = Number.POSITIVE_INFINITY; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2= Infinity; VAR1 += VAR2;VAR1",    Number.POSITIVE_INFINITY,       VAR1 );
    VAR1 = -0; VAR2 = Number.POSITIVE_INFINITY; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = -0; VAR2= Infinity; VAR1 += VAR2;VAR1",   Number.POSITIVE_INFINITY,       VAR1 );
     VAR1 = -0; VAR2 = Number.NEGATIVE_INFINITY; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = -0; VAR2= -Infinity; VAR1 += VAR2;VAR1",  Number.NEGATIVE_INFINITY,      VAR1 );
    VAR1 = 0; VAR2 = Number.NEGATIVE_INFINITY; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2= -Infinity; VAR1 += VAR2;VAR1",   Number.NEGATIVE_INFINITY,       VAR1 );

    // the sum of two negative zeros is -0. the sum of two positive zeros, or of two zeros of opposite sign, is +0
    VAR1 = 0; VAR2 = 0; VAR1 += VAR2; 
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2= 0; VAR1 += VAR2",    0,      VAR1 );
     VAR1 = 0; VAR2 = -0; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2= -0; VAR1 += VAR2",   0,      VAR1 );
    VAR1 = -0; VAR2 = 0; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = -0; VAR2= 0; VAR1 += VAR2",   0,       VAR1 );
    VAR1 = -0; VAR2 = -0; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = -0; VAR2= -0; VAR1 += VAR2",  -0,       VAR1 );

    //  the sum of a zero and a nonzero finite value is eqal to the nonzero operand
    VAR1 = 0; VAR2 = 1; VAR2 += VAR1;
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2= 1; VAR2 += VAR1; VAR2",    1,       VAR2 );
    VAR1 = -0; VAR2 = 1; VAR2 += VAR1;
    array[item++] = new TestCase( SECTION,    "VAR1 = -0; VAR2= 1; VAR2 += VAR1; VAR2",   1,       VAR2 );
    VAR1 = -0; VAR2 = -1; VAR2 += VAR1; 
    array[item++] = new TestCase( SECTION,    "VAR1 = -0; VAR2= -1; VAR2 += VAR1; VAR2",  -1,      VAR2 );
    VAR1 = 0; VAR2 = -1; VAR2 += VAR1;
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2= -1; VAR2 += VAR1; VAR2",   -1,       VAR2 );

    // the sum of a zero and a nozero finite value is equal to the nonzero operand.
     VAR1 = 0; VAR2=1; 
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2=1; VAR1 += VAR2",         1,         VAR1 += VAR2 );
      VAR1 = 0; VAR2=1; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = 0; VAR2=1; VAR1 += VAR2;VAR1",    1,        VAR1 );

    // the sum of two nonzero finite values of the same magnitude and opposite sign is +0
    VAR1 = Number.MAX_VALUE; VAR2= -Number.MAX_VALUE; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = Number.MAX_VALUE; VAR2= -Number.MAX_VALUE; VAR1 += VAR2; VAR1",    0,   VAR1 );
     VAR1 = Number.MIN_VALUE; VAR2= -Number.MIN_VALUE; VAR1 += VAR2; 
    array[item++] = new TestCase( SECTION,    "VAR1 = Number.MIN_VALUE; VAR2= -Number.MIN_VALUE; VAR1 += VAR2; VAR1",   0, VAR1 );

/*
    VAR1 = 10; VAR2 = '0XFF';
    array[item++] = new TestCase( SECTION,    "VAR1 = 10; VAR2 = '0XFF', VAR1 += VAR2", 2550,       VAR1 += VAR2 );
    VAR1 = '0XFF'; VAR2 = 0XA; 
    array[item++] = new TestCase( SECTION,    "VAR1 = '0xFF'; VAR2 = 0xA, VAR1 += VAR2", 2550,      VAR1 += VAR2 );
    VAR1 = '10'; VAR2 = '255';
    array[item++] = new TestCase( SECTION,    "VAR1 = '10'; VAR2 = '255', VAR1 += VAR2", 2550,      VAR1 += VAR2 );
    VAR1 = '10'; VAR2 = '0XFF'; 
    array[item++] = new TestCase( SECTION,    "VAR1 = '10'; VAR2 = '0XFF', VAR1 += VAR2", 2550,     VAR1 += VAR2 );
    VAR1 = '0XFF'; VAR2 = 0XA;
    array[item++] = new TestCase( SECTION,    "VAR1 = '0xFF'; VAR2 = 0xA, VAR1 += VAR2", 2550,      VAR1 += VAR2 );

    // boolean cases
    VAR1 = true; VAR2 = false; 
    array[item++] = new TestCase( SECTION,    "VAR1 = true; VAR2 = false; VAR1 += VAR2",    0,      VAR1 += VAR2 );
     VAR1 = true; VAR2 = true;
    array[item++] = new TestCase( SECTION,    "VAR1 = true; VAR2 = true; VAR1 += VAR2",    1,      VAR1 += VAR2 );

    // object cases
    VAR1 = new Boolean(true); VAR2 = 10; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = new Boolean(true); VAR2 = 10; VAR1 += VAR2;VAR1",    10,       VAR1 );
    VAR1 = new Number(11); VAR2 = 10; VAR1 += VAR2;
    array[item++] = new TestCase( SECTION,    "VAR1 = new Number(11); VAR2 = 10; VAR1 += VAR2; VAR1",    110,       VAR1 );
    VAR1 = new Number(11); VAR2 = new Number(10);
    array[item++] = new TestCase( SECTION,    "VAR1 = new Number(11); VAR2 = new Number(10); VAR1 += VAR2",    110,       VAR1 += VAR2 );
    VAR1 = String('15'); VAR2 = new String('0xF');
    array[item++] = new TestCase( SECTION,    "VAR1 = new String('15'); VAR2 = new String('0xF'); VAR1 += VAR2",    255,       VAR1 += VAR2 );

*/
    return ( array );
}
