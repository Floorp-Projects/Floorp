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
    var SECTION = "15.8.2.1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Math.abs()";
    var BUGNUMBER = "77391";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,   "Math.abs.length",             1,              Math.abs.length );
    var thisError="no error";
    try{
        Math.abs();
    }catch(e:Error){
        thisError=(e.toString()).substring(0,26);
    }finally{
        array[item++] = new TestCase( SECTION,   "Math.abs()","ArgumentError: Error #1063",thisError);
    }
  
    array[item++] = new TestCase( SECTION,   "Math.abs( void 0 )",          Number.NaN,     Math.abs(void 0) );
    array[item++] = new TestCase( SECTION,   "Math.abs( null )",            0,              Math.abs(null) );
    array[item++] = new TestCase( SECTION,   "Math.abs( true )",            1,              Math.abs(true) );
    array[item++] = new TestCase( SECTION,   "Math.abs( false )",           0,              Math.abs(false) );
    array[item++] = new TestCase( SECTION,   "Math.abs( string primitive)", Number.NaN,     Math.abs("a string primitive")                  );
    array[item++] = new TestCase( SECTION,   "Math.abs( string object )",   Number.NaN,     Math.abs(new String( 'a String object' ))       );
    array[item++] = new TestCase( SECTION,   "Math.abs( Number.NaN )",      Number.NaN,     Math.abs(Number.NaN) );

    array[item++] = new TestCase( SECTION,   "Math.abs(0)",                 0,              Math.abs( 0 )                                   );
    array[item++] = new TestCase( SECTION,   "Math.abs( -0 )",              0,              Math.abs(-0) );
    array[item++] = new TestCase( SECTION,   "Infinity/Math.abs(-0)",      Infinity,        Infinity/Math.abs(-0) );

    array[item++] = new TestCase( SECTION,   "Math.abs( -Infinity )",       Number.POSITIVE_INFINITY,   Math.abs( Number.NEGATIVE_INFINITY ) );
    array[item++] = new TestCase( SECTION,   "Math.abs( Infinity )",        Number.POSITIVE_INFINITY,   Math.abs( Number.POSITIVE_INFINITY ) );
    array[item++] = new TestCase( SECTION,   "Math.abs( - MAX_VALUE )",     Number.MAX_VALUE,           Math.abs( - Number.MAX_VALUE )       );
    array[item++] = new TestCase( SECTION,   "Math.abs( - MIN_VALUE )",     Number.MIN_VALUE,           Math.abs( -Number.MIN_VALUE )        );
    array[item++] = new TestCase( SECTION,   "Math.abs( MAX_VALUE )",       Number.MAX_VALUE,           Math.abs( Number.MAX_VALUE )       );
    array[item++] = new TestCase( SECTION,   "Math.abs( MIN_VALUE )",       Number.MIN_VALUE,           Math.abs( Number.MIN_VALUE )        );

    array[item++] = new TestCase( SECTION,   "Math.abs( -1 )",               1,                          Math.abs( -1 )                       );
    array[item++] = new TestCase( SECTION,   "Math.abs( new Number( -1 ) )", 1,                          Math.abs( new Number(-1) )           );
    array[item++] = new TestCase( SECTION,   "Math.abs( 1 )",                1,                          Math.abs( 1 ) );
    array[item++] = new TestCase( SECTION,   "Math.abs( Math.PI )",          Math.PI,                    Math.abs( Math.PI ) );
    array[item++] = new TestCase( SECTION,   "Math.abs( -Math.PI )",         Math.PI,                    Math.abs( -Math.PI ) );
    array[item++] = new TestCase( SECTION,   "Math.abs(-1/100000000)",       1/100000000,                Math.abs(-1/100000000) );
    array[item++] = new TestCase( SECTION,   "Math.abs(-Math.pow(2,32))",    Math.pow(2,32),             Math.abs(-Math.pow(2,32)) );
    array[item++] = new TestCase( SECTION,   "Math.abs(Math.pow(2,32))",     Math.pow(2,32),             Math.abs(Math.pow(2,32)) );
    array[item++] = new TestCase( SECTION,   "Math.abs( -0xfff )",           4095,                       Math.abs( -0xfff ) );
    array[item++] = new TestCase( SECTION,   "Math.abs( -0777 )",            777,                        Math.abs(-0777 ) );

    array[item++] = new TestCase( SECTION,   "Math.abs('-1e-1')",           0.1,            Math.abs('-1e-1') );
    array[item++] = new TestCase( SECTION,   "Math.abs('0xff')",            255,            Math.abs('0xff') );
    array[item++] = new TestCase( SECTION,   "Math.abs('077')",             77,             Math.abs('077') );
    array[item++] = new TestCase( SECTION,   "Math.abs( 'Infinity' )",      Infinity,       Math.abs('Infinity') );
    array[item++] = new TestCase( SECTION,   "Math.abs( '-Infinity' )",     Infinity,       Math.abs('-Infinity') );

    array[item++] = new TestCase( SECTION,    "Math.abs(1)", "1",     Math.abs(1)+"");

    array[item++] = new TestCase( SECTION,    "Math.abs(10)", "10",     Math.abs(10)+"");
  
    array[item++] = new TestCase( SECTION,    "Math.abs(100)", "100",     Math.abs(100)+"");
 
    array[item++] = new TestCase( SECTION,    "Math.abs(1000)", "1000",     Math.abs(1000)+"");
  
    array[item++] = new TestCase( SECTION,    "Math.abs(10000)", "10000",     Math.abs(10000)+"");
   
    array[item++] = new TestCase( SECTION,    "Math.abs(10000000000)", "10000000000",Math.abs(10000000000)+"");
   
    array[item++] = new TestCase( SECTION,    "Math.abs(10000000000000000000)", "10000000000000000000",Math.abs(10000000000000000000)+"");
   
    array[item++] = new TestCase( SECTION,    "Math.abs(100000000000000000000)", "100000000000000000000",Math.abs(100000000000000000000)+"" );
   
    array[item++] = new TestCase( SECTION,    "Math.abs(-12345 )", "12345",Math.abs(-12345)+"");
  
    array[item++] = new TestCase( SECTION,    "Math.abs(-1234567890)", "1234567890",Math.abs(-1234567890)+"");
   
    array[item++] = new TestCase( SECTION,    "Math.abs(-10 )", "10",Math.abs(-10)+"");   
   
    array[item++] = new TestCase( SECTION,    "Math.abs(-100 )", "100",Math.abs(-100)+"");   
    array[item++] = new TestCase( SECTION,    "Math.abs(-1000 )", "1000",Math.abs(-1000)+""); 
   
    array[item++] = new TestCase( SECTION,    "Math.abs(-1000000000 )", "1000000000",Math.abs(-1000000000 )+"");
   
    array[item++] = new TestCase( SECTION,    "Math.abs(-1000000000000000)", "1000000000000000",Math.abs(-1000000000000000)+"");
   
    array[item++] = new TestCase( SECTION,    "Math.abs(-100000000000000000000)", "100000000000000000000",Math.abs(-100000000000000000000)+"");
    
   
    array[item++] = new TestCase( SECTION,    "Math.abs(-1000000000000000000000)", "1e+21",Math.abs(-1000000000000000000000)+"" );
    
    array[item++] = new TestCase( SECTION,    "Math.abs(1.0000001)", "1.0000001",Math.abs(1.0000001 )+"");
   
    array[item++] = new TestCase( SECTION,    "Math.abs(1000000000000000000000)", "1e+21",Math.abs(1000000000000000000000)+"");

   
    array[item++] = new TestCase( SECTION,    "Math.abs(1.2345)", "1.2345",Math.abs(1.2345)+"" );

   
    array[item++] = new TestCase( SECTION,    "Math.abs(1.234567890)", "1.23456789",Math.abs(1.234567890)+"");

   
    array[item++] = new TestCase( SECTION,    "Math.abs(.12345)", "0.12345",Math.abs(.12345)+"");

    array[item++] = new TestCase( SECTION,    "Math.abs(.012345)", "0.012345",Math.abs(.012345)+"" );
    
   
    array[item++] = new TestCase( SECTION,    "Math.abs(.00012345)", "0.00012345",Math.abs(.00012345)+"");
   
    array[item++] = new TestCase( SECTION,    "Math.abs(.000012345)", "0.000012345",Math.abs(.000012345)+"" );
   
    array[item++] = new TestCase( SECTION,    "Math.abs(.0000012345)", "0.0000012345",Math.abs(.0000012345)+"");
   
    array[item++] = new TestCase( SECTION,    "Math.abs(.00000012345)", "1.2345e-7",Math.abs(.00000012345)+"");

    return ( array );
}
