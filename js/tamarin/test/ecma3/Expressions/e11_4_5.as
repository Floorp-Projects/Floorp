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
    var SECTION = "e11_4_5";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " Prefix decrement operator");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    //
    var MYVAR;
    array[item++] = new TestCase( SECTION,  "var MYVAR; --MYVAR",                       NaN,                             --MYVAR );
    var MYVAR=void 0;
    array[item++] = new TestCase( SECTION,  "var MYVAR= void 0; --MYVAR",               NaN,                             --MYVAR );
    var MYVAR=null;
    array[item++] = new TestCase( SECTION,  "var MYVAR=null; --MYVAR",                  -1,                             --MYVAR );
    var MYVAR=true; 
    array[item++] = new TestCase( SECTION,  "var MYVAR=true; --MYVAR",                  0,                            --MYVAR );
    var MYVAR=false;
    array[item++] = new TestCase( SECTION,  "var MYVAR=false; --MYVAR",                 -1,                             --MYVAR );

    // special numbers
    // verify return value
     var MYVAR=Number.POSITIVE_INFINITY;
    array[item++] = new TestCase( SECTION,    "var MYVAR=Number.POSITIVE_INFINITY;--MYVAR", Number.POSITIVE_INFINITY,  --MYVAR );
    var MYVAR=Number.NEGATIVE_INFINITY;
    array[item++] = new TestCase( SECTION,    "var MYVAR=Number.NEGATIVE_INFINITY;--MYVAR", Number.NEGATIVE_INFINITY,   --MYVAR );
     var MYVAR=Number.NaN;
    array[item++] = new TestCase( SECTION,    "var MYVAR=Number.NaN;--MYVAR",               Number.NaN,                --MYVAR );

    // verify value of variable
    var MYVAR=Number.POSITIVE_INFINITY;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=Number.POSITIVE_INFINITY;--MYVAR;MYVAR", Number.POSITIVE_INFINITY,   MYVAR );
    var MYVAR=Number.NEGATIVE_INFINITY;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=Number.NEGATIVE_INFINITY;--MYVAR;MYVAR", Number.NEGATIVE_INFINITY,   MYVAR );
    var MYVAR=Number.NaN;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=Number.NaN;--MYVAR;MYVAR",               Number.NaN,                 MYVAR );


    // number primitives
    var MYVAR=0;
    array[item++] = new TestCase( SECTION,    "var MYVAR=0;--MYVAR",            -1,         --MYVAR );
    var MYVAR=0.2345;
    array[item++] = new TestCase( SECTION,    "var MYVAR=0.2345;--MYVAR",      -0.7655000000000001,     --MYVAR );
    var MYVAR=-0.2345;
    array[item++] = new TestCase( SECTION,    "var MYVAR=-0.2345;--MYVAR",      -1.2345,    --MYVAR );

    // verify value of variable
    var MYVAR=0;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=0;--MYVAR;MYVAR",      -1,         MYVAR );
    var MYVAR=0.2345;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=0.2345;--MYVAR;MYVAR", -0.7655000000000001,    MYVAR );
    var MYVAR=-0.2345;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=-0.2345;--MYVAR;MYVAR", -1.2345,   MYVAR );
    var MYVAR=0;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=0;--MYVAR;MYVAR",      -1,   MYVAR );
    var MYVAR=0;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=0;--MYVAR;MYVAR",      -1,   MYVAR );
    var MYVAR=0;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=0;--MYVAR;MYVAR",      -1,   MYVAR );

    // boolean values
    // verify return value
    var MYVAR=true;
    array[item++] = new TestCase( SECTION,    "var MYVAR=true;--MYVAR",         0,       --MYVAR );
    var MYVAR=false;
    array[item++] = new TestCase( SECTION,    "var MYVAR=false;--MYVAR",        -1,      --MYVAR );
    // verify value of variable
    var MYVAR=true;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=true;--MYVAR;MYVAR",   0,   MYVAR );
    var MYVAR=false;--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=false;--MYVAR;MYVAR",  -1,   MYVAR );

    // boolean objects
    // verify return value
    var MYVAR=true;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new Boolean(true);--MYVAR",         0,     --MYVAR );
    var MYVAR=false;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new Boolean(false);--MYVAR",        -1,     --MYVAR );
    // verify value of variable
    var MYVAR=new Boolean(true);--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new Boolean(true);--MYVAR;MYVAR",   0,     MYVAR );
    var MYVAR=new Boolean(false);--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new Boolean(false);--MYVAR;MYVAR",  -1,     MYVAR );

    // string primitives
    var MYVAR='string';
    array[item++] = new TestCase( SECTION,    "var MYVAR='string';--MYVAR",         Number.NaN,     --MYVAR );
    var MYVAR='12345';
    array[item++] = new TestCase( SECTION,    "var MYVAR='12345';--MYVAR",          12344,          --MYVAR );
    var MYVAR='-12345';
    array[item++] = new TestCase( SECTION,    "var MYVAR='-12345';--MYVAR",         -12346,         --MYVAR );
    var MYVAR='0Xf';
    array[item++] = new TestCase( SECTION,    "var MYVAR='0Xf';--MYVAR",            14,             --MYVAR );
    var MYVAR='077';
    array[item++] = new TestCase( SECTION,    "var MYVAR='077';--MYVAR",            76,             --MYVAR );
    var MYVAR='';
    array[item++] = new TestCase( SECTION,    "var MYVAR=''; --MYVAR",              -1,              --MYVAR );

    // verify value of variable
     var MYVAR='string';--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR='string';--MYVAR;MYVAR",   Number.NaN,    MYVAR );
    var MYVAR='12345';--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR='12345';--MYVAR;MYVAR",    12344,          MYVAR );
    var MYVAR='-12345';--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR='-12345';--MYVAR;MYVAR",   -12346,          MYVAR );
    var MYVAR='0xf';--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR='0xf';--MYVAR;MYVAR",      14,             MYVAR );
    var MYVAR='077';--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR='077';--MYVAR;MYVAR",      76,             MYVAR );
    var MYVAR='';--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR='';--MYVAR;MYVAR",         -1,              MYVAR );

    // string objects
    var MYVAR=new String('string');
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('string');--MYVAR",         Number.NaN,     --MYVAR );
    var MYVAR=new String('12345');
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('12345');--MYVAR",          12344,          --MYVAR );
    var MYVAR=new String('-12345');
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('-12345');--MYVAR",         -12346,         --MYVAR );
    var MYVAR=new String('0Xf');
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('0Xf');--MYVAR",            14,             --MYVAR );
    var MYVAR=new String('077');
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('077');--MYVAR",            76,             --MYVAR );
    var MYVAR=new String('');
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String(''); --MYVAR",              -1,              --MYVAR );

    // verify value of variable
    var MYVAR=new String('string');--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('string');--MYVAR;MYVAR",   Number.NaN,     MYVAR );
    var MYVAR=new String('12345');--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('12345');--MYVAR;MYVAR",    12344,          MYVAR );
    var MYVAR=new String('-12345');--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('-12345');--MYVAR;MYVAR",   -12346,          MYVAR );
    var MYVAR=new String('0xf');--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('0xf');--MYVAR;MYVAR",      14,             MYVAR );
    var MYVAR=new String('077');--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('077');--MYVAR;MYVAR",      76,             MYVAR );
     var MYVAR=new String('');--MYVAR;
    array[item++] = new TestCase( SECTION,    "var MYVAR=new String('');--MYVAR;MYVAR",         -1,             MYVAR );

    return ( array );
}
