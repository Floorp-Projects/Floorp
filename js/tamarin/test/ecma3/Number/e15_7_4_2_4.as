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
    var SECTION = "15.7.4.2-4";
    var VERSION = "ECMA_4";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " Number.prototype.toString()");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

	
    var o = 3; 
    array[item++] = new TestCase(SECTION,"o = 3;o.toString()",             
						"3",    
						o.toString() );

    var o = new Number(3); 
    array[item++] = new TestCase(SECTION, "o = new Number(3);o.toString()",             
						"3",    
						o.toString() );

    var o = new Number(); 
    array[item++] = new TestCase(SECTION,"o = new Number();o.toString()",             
						"0",    
						o.toString() );

   
    var o = new Number(3); 
    array[item++] = new TestCase(SECTION,"o = new Number(3);o.toString()",             
						"3",    
						o.toString(10) );

    o = new Number(3);
    array[item++] = new TestCase(SECTION,"o = new Number(3);o.toString()",             
						"11",    
						o.toString(2) );

    o = new Number(3);
    array[item++] = new TestCase(SECTION,"o = new Number(3);o.toString()",             
						"3",    
						o.toString(8) );

    o = new Number(11);
    array[item++] = new TestCase(SECTION,"o = new Number(11);o.toString()",             
						"13",    
						o.toString(8) );
   
    o= new Number(3);
    array[item++] = new TestCase(SECTION,"o = new Number(3);o.toString()",             
						"3",    
						o.toString(16) );

    o= new Number(17);
    array[item++] = new TestCase(SECTION,"o = new Number(17);o.toString()",             
						"11",    
						o.toString(16) );

    o=new Number(true);

    array[item++] = new TestCase( SECTION,	"o=new Number(true)",			    "1",o.toString() );
    
    o=new Number(false);
    array[item++] = new TestCase( SECTION,	"o=new Number(false)",		    "0",		o.toString() );
    o=new Number(new Array());
    array[item++] = new TestCase( SECTION,	"o=new Number(new Array())",		"0",			    o.toString() );
    o=Number.NaN;
    array[item++] = new TestCase( SECTION,    "o=Number.NaN;o.toString()",       "NaN",                  o.toString() );
    o=new Number(0)
    array[item++] = new TestCase( SECTION,    "o=0;o.toString()",                "0",                    o.toString());
    o=new Number(-0);
    array[item++] = new TestCase( SECTION,    "o=-0;o.toString()",               "0",                   o.toString() );

    o=new Number(Number.POSITIVE_INFINITY);
    array[item++] = new TestCase( SECTION,    "o=new Number(Number.POSITIVE_INFINITY)", "Infinity",     o.toString() );
    o=new Number(Number.NEGATIVE_INFINITY);
    array[item++] = new TestCase( SECTION,    "o=new Number(Number.NEGATIVE_INFINITY);o.toString()", "-Infinity",     o.toString() );
  
    o=new Number(-1);
    array[item++] = new TestCase( SECTION,    "o=new Number(-1);o.toString()", "-1",     o.toString() );

    // cases in step 6:  integers  1e21 > x >= 1 or -1 >= x > -1e21
   
    o=new Number(1);
    array[item++] = new TestCase( SECTION,    "o=new Number(1);o.toString()", "1",     o.toString() );
    o=new Number(10);
    array[item++] = new TestCase( SECTION,    "o=new Number(10);o.toString()", "10",     o.toString() );
    o=new Number(100);
    array[item++] = new TestCase( SECTION,    "o=new Number(100);o.toString()", "100",     o.toString() );
    o=new Number(1000);
    array[item++] = new TestCase( SECTION,    "o=new Number(1000);o.toString()", "1000",     o.toString() );
    o=new Number(10000);
    array[item++] = new TestCase( SECTION,    "o=new Number(10000);o.toString()", "10000",     o.toString() );
    o=new Number(10000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(10000000000);o.toString()", "10000000000",o.toString() );
    o=new Number(10000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(10000000000000000000);o.toString()", "10000000000000000000",o.toString() );
    o=new Number(100000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(100000000000000000000);o.toString()", "100000000000000000000",o.toString() );
    o=new Number(12345 );
    array[item++] = new TestCase( SECTION,    "o=new Number(12345 );o.toString()", "12345",o.toString() );
    o=new Number(1234567890 );
    array[item++] = new TestCase( SECTION,    "o=new Number(1234567890);o.toString()", "1234567890",o.toString() );
    o=new Number(-1);
    array[item++] = new TestCase( SECTION,    "o=new Number(-1);o.toString()", "-1",o.toString() );
    o=new Number(-10 );
    array[item++] = new TestCase( SECTION,    "o=new Number(-10 );o.toString()", "-10",o.toString() );
   
    o=new Number(-100 );
    array[item++] = new TestCase( SECTION,    "o=new Number(-100 );o.toString()", "-100",o.toString() );
    o=new Number(-1000 );
    array[item++] = new TestCase( SECTION,    "o=new Number(-1000 );o.toString()", "-1000",o.toString() ); 
    o=new Number(-1000000000 );
    array[item++] = new TestCase( SECTION,    "o=new Number(-1000000000 );o.toString()", "-1000000000",o.toString() );
    o=new Number(-1000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(-1000000000000000);o.toString()", "-1000000000000000",o.toString() );
    o=new Number(-100000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(-100000000000000000000);o.toString()", "-100000000000000000000",o.toString() );
    
    o=new Number(-1000000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(-1000000000000000000000);o.toString()", "-1e+21",o.toString() );
    
    o=new Number(1.0000001 );
    array[item++] = new TestCase( SECTION,    "o=new Number(1.0000001);o.toString()", "1.0000001",o.toString() );
    o=new Number(1000000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(1000000000000000000000);o.toString()", "1e+21",o.toString() );

    o=new Number(1.2345);
    array[item++] = new TestCase( SECTION,    "o=new Number(1.2345);o.toString()", "1.2345",o.toString() );

    o=new Number(1.234567890);
    array[item++] = new TestCase( SECTION,    "o=new Number(1.234567890);o.toString()", "1.23456789",o.toString() );

    o=new Number(.12345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.12345);o.toString()", "0.12345",o.toString() );

    o=new Number(.012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.012345);o.toString()", "0.012345",o.toString() );
    
    o=new Number(.0012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.0012345);o.toString()", "0.0012345",o.toString() );
    o=new Number(.00012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.00012345);o.toString()", "0.00012345",o.toString() );
    o=new Number(.000012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.000012345);o.toString()", "0.000012345",o.toString() );
    o=new Number(.0000012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.0000012345);o.toString()", "0.0000012345",o.toString() );
    o=new Number(.00000012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.00000012345);o.toString()", "1.2345e-7",o.toString() );
    o=new Number();
    array[item++] = new TestCase( SECTION,    "o=new Number();o.toString()", "0",o.toString() );
      
    var date = new Date(0);
    var thisError="no error";
    try{
        date.myToString=Number.prototype.toString
        date.myToString();
    }catch(e:TypeError){
        thisError = e.toString()
    }
   
    array[item++] = new TestCase(SECTION,  
						"date.myToString=Number.prototype.toString;date.myToString()",             
						"TypeError: Error #1004",    
						typeError(thisError) );

    var o = new Number(3);

    try{
        o.toString(1);
    }catch(e:RangeError){
        thisError=e.toString();
    }

    array[item++] = new TestCase(SECTION,  
						"var o=new Number(3);o.toString(1)","RangeError: Error #1003",    
						rangeError(thisError) );

    var o = new Number(3);

    try{
        o.toString(37);
    }catch(e:RangeError){
        thisError=e.toString();
    }

    array[item++] = new TestCase(SECTION,  
						"var o=new Number(3);o.toString(37)","RangeError: Error #1003",    
						rangeError(thisError) );

   

    return ( array );
}
