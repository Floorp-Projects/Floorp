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


    var SECTION = "15.8.2.8";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Math.exp(x)";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,   "Math.exp.length",     1,              Math.exp.length );
    var thisError="no error";
    try{
        Math.exp();
    }catch(e:Error){
        thisError=(e.toString()).substring(0,26);
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,   "Math.exp()","ArgumentError: Error #1063",thisError);
    }
  /*array[item++] = new TestCase( SECTION,   "Math.exp()",          Number.NaN,     Math.exp() );*/
    array[item++] = new TestCase( SECTION,   "Math.exp(null)",      1,              Math.exp(null) );
    array[item++] = new TestCase( SECTION,   "Math.exp(void 0)",    Number.NaN,     Math.exp(void 0) );
    array[item++] = new TestCase( SECTION,   "Math.exp(1)",          Math.E,         Math.exp(1) );
    
    array[item++] = new TestCase( SECTION,   "Math.exp(true)",      Math.E,         Math.exp(true) );
    array[item++] = new TestCase( SECTION,   "Math.exp(false)",     1,              Math.exp(false) );

    array[item++] = new TestCase( SECTION,   "Math.exp('1')",       Math.E,         Math.exp('1') );
    array[item++] = new TestCase( SECTION,   "Math.exp('0')",       1,              Math.exp('0') );

    array[item++] = new TestCase( SECTION,   "Math.exp(NaN)",       Number.NaN,      Math.exp(Number.NaN) );
    array[item++] = new TestCase( SECTION,   "Math.exp(0)",          1,              Math.exp(0)          );
    array[item++] = new TestCase( SECTION,   "Math.exp(-0)",         1,              Math.exp(-0)         );
    array[item++] = new TestCase( SECTION,   "Math.exp(Infinity)",   Number.POSITIVE_INFINITY,   Math.exp(Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,   "Math.exp(-Infinity)",  0,              Math.exp(Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,   "Math.exp(2)",  7.38905609893065,              Math.exp(2) );

	
	// test order number 15
	// [amemon 9/14/2006] This test case breaks on mac PPC and linux because of an OS precision error. Fixing... 
	// array[item++] = new TestCase( SECTION,   "Math.exp(10)",  22026.465794806725,              Math.exp(10) );
	var maxCorrect15:Number = 22026.4657948068;
	var minCorrect15:Number = 22026.4657948067;
	
	array[item++] = new TestCase( SECTION,   "Math.exp(10)",  true, 
									(Math.exp(10) > minCorrect15 && Math.exp(10) < maxCorrect15) );
	
	
	
	
	// test order number 16
	// [amemon 9/14/2006] This test case breaks on mac PPC and linux because of an OS precision error. Fixing... 
	// array[item++] = new TestCase( SECTION,   "Math.exp(100)","2.68811714181616e+43",              Math.exp(100)+"" );
	array[item++] = new TestCase( SECTION,   "Math.exp(100)", true, 
									( 
										(String(Math.exp(100)+"").indexOf("2.6881171418161") != -1) && 
										(String(Math.exp(100)+"").indexOf("e+43") != -1)
									)    
								);
    
    
    
    
    
    
    array[item++] = new TestCase( SECTION,   "Math.exp(1000)",Infinity,              Math.exp(1000));
    array[item++] = new TestCase( SECTION,   "Math.exp(-1000)",0,              Math.exp(-1000));  
    array[item++] = new TestCase( SECTION,   "Math.exp(100000)",Infinity,              Math.exp(100000));
    array[item++] = new TestCase( SECTION,   "Math.exp(Number.MAX_VALUE)",Infinity,              Math.exp(Number.MAX_VALUE));
    array[item++] = new TestCase( SECTION,   "Math.exp(Number.MIN_VALUE)",1,              Math.exp(Number.MIN_VALUE));
    
    return ( array );
}
