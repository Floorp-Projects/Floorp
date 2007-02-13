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
	
	
    var SECTION = "15.8.2.2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Math.acos()";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "Math.acos.length",         1,              Math.acos.length );

    array[item++] = new TestCase( SECTION,  "Math.acos(void 0)",        Number.NaN,     Math.acos(void 0) );

  /*  thisError="no error";
   try{
      Math.acos();
   }catch(e:Error){
       thisError=(e.toString()).substring(0,26);
   }finally{//print(thisError);
       array[item++] = new TestCase( SECTION,   "Math.acos()","ArgumentError: Error #1063",thisError);
    }*/
  /*array[item++] = new TestCase( SECTION,  "Math.acos()",              Number.NaN,     Math.acos() );*/
    array[item++] = new TestCase( SECTION,  "Math.acos(null)",          Math.PI/2,      Math.acos(null) );
    array[item++] = new TestCase( SECTION,  "Math.acos(NaN)",           Number.NaN,     Math.acos(Number.NaN) );

    array[item++] = new TestCase( SECTION,  "Math.acos(a string)",      Number.NaN,     Math.acos("a string") );
    array[item++] = new TestCase( SECTION,  "Math.acos('0')",           Math.PI/2,      Math.acos('0') );
    array[item++] = new TestCase( SECTION,  "Math.acos('1')",           0,              Math.acos('1') );
    array[item++] = new TestCase( SECTION,  "Math.acos('-1')",          Math.PI,        Math.acos('-1') );

    array[item++] = new TestCase( SECTION,  "Math.acos(1.00000001)",    Number.NaN,     Math.acos(1.00000001) );
    array[item++] = new TestCase( SECTION,  "Math.acos(11.00000001)",   Number.NaN,     Math.acos(-1.00000001) );
    array[item++] = new TestCase( SECTION,  "Math.acos(1)",    	        0,              Math.acos(1)          );
    array[item++] = new TestCase( SECTION,  "Math.acos(-1)",            Math.PI,        Math.acos(-1)         );
    array[item++] = new TestCase( SECTION,  "Math.acos(0)",    	        Math.PI/2,      Math.acos(0)          );
    array[item++] = new TestCase( SECTION,  "Math.acos(-0)",   	        Math.PI/2,      Math.acos(-0)         );
    array[item++] = new TestCase( SECTION,  "Math.acos(Math.SQRT1_2)",	Math.PI/4,      Math.acos(Math.SQRT1_2));
    array[item++] = new TestCase( SECTION,  "Math.acos(-Math.SQRT1_2)", Math.PI/4*3,    Math.acos(-Math.SQRT1_2));
	
	
	
	
	// test order number 16
	// [amemon 9/14/2006] This test case breaks on mac PPC and linux because of an OS precision error. Fixing... 
	// Linux gives this: 0.008726646256686873
	// Mac probably gives the same as linux
	// Windows gives this: 0.008726646256688278
	// changing from the following to that which follows the following... 
	// array[item++] = new TestCase( SECTION,  "Math.acos(0.9999619230642)",	0.008726646256688278,    Math.acos(0.9999619230642));
	
	var maxAcceptable16:Number = 0.00872664625669;
	var minAcceptable16:Number = 0.00872664625668;
	
	array[item++] = new TestCase( SECTION,  "Math.acos(0.9999619230642)", true, 
									(
										Math.acos(0.9999619230642) > minAcceptable16 && 
										Math.acos(0.9999619230642) < maxAcceptable16
									)
								);
    
    
    

    return ( array );
}
