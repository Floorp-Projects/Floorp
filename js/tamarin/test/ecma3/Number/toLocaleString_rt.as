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
    var SECTION = "15.7.4.3";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " Number.prototype.toLocaleString()");
    test();

function getTestCases() {
    var array:Array = new Array();
    var item:Number= 0;
    var o:Number = new Number();
	
    array[item++] = new TestCase(SECTION, "Number.prototype.toLocaleString()",       "0",        Number.prototype.toLocaleString() );
    array[item++] = new TestCase(SECTION, "typeof(Number.prototype.toLocaleString())", "string",      typeof(Number.prototype.toLocaleString()) );
    var s:Number=new Number();

	var thisError:String = "no exception thrown";
	try{
    	s = Number.prototype.toLocaleString; 
    	 
    	o.toLocaleString = s;
	} catch (e:ReferenceError) {
		thisError = e.toString();
	} finally {
    	array[item++] = new TestCase(SECTION,  "s = Number.prototype.toLocaleString; o = new Number(); o.toLocaleString = s; o.toLocaleString()",  
												"ReferenceError: Error #1056",          
												referenceError( thisError ) );
	}

	thisError = "no exception thrown";
        var o:Number = new Number(1);
	try{
    	s = Number.prototype.toLocaleString; 
    	 
    	o.toLocaleString = s;
	} catch(e1:ReferenceError) {
		thisError = e1.toString();
	} finally {
    	array[item++] = new TestCase(SECTION,  "s = Number.prototype.toLocaleString; o = new Number(1); o.toLocaleString = s; o.toLocaleString()", 
												"ReferenceError: Error #1056",          
												referenceError( thisError) );
	}

	thisError = "no exception thrown";
        var o:Number= new Number(-1);
	try{
    	s = Number.prototype.toLocaleString; 
    	 
    	o.toLocaleString = s; 
	} catch (e2:ReferenceError) {
		thisError = e2.toString();
	} finally {
    	array[item++] = new TestCase(SECTION,  "s = Number.prototype.toLocaleString; o = new Number(-1); o.toLocaleString = s; o.toLocaleString()", 
												"ReferenceError: Error #1056",         
												referenceError(thisError) );
	}

    var MYNUM = new Number(255);
    array[item++] = new TestCase(SECTION, "var MYNUM = new Number(255); MYNUM.toLocaleString()",          "255",      MYNUM.toLocaleString() );

    var MYNUM = new Number(Number.NaN);
    array[item++] = new TestCase(SECTION, "var MYNUM = new Number(Number.NaN); MYNUM.toLocaleString()",   "NaN",      MYNUM.toLocaleString() );

    var MYNUM = new Number(Infinity);
    array[item++] = new TestCase(SECTION, "var MYNUM = new Number(Infinity); MYNUM.toLocaleString()",   "Infinity",   MYNUM.toLocaleString() );

    var MYNUM = new Number(-Infinity);
    array[item++] = new TestCase(SECTION, "var MYNUM = new Number(-Infinity); MYNUM.toLocaleString()",   "-Infinity", MYNUM.toLocaleString() );

    var o=new Number(true);

    array[item++] = new TestCase( SECTION,	"o=new Number(true);o.toLocaleString()",			    "1",o.toLocaleString() );
    
    o=new Number(false);
    array[item++] = new TestCase( SECTION,	"o=new Number(false);o.toLocaleString()",		    "0",		o.toLocaleString() );
    o=new Number(new Array());
    array[item++] = new TestCase( SECTION,	"o=new Number(new Array());o.toLocaleString()",		"0",			    o.toLocaleString() );
    o=new Number(Number.NaN);
    array[item++] = new TestCase( SECTION,    "o=Number.NaN;o.toLocaleString()",       "NaN",                  o.toLocaleString() );
    o=new Number(0)
    array[item++] = new TestCase( SECTION,    "o=0;o.toLocaleString()",                "0",                    o.toLocaleString());
    o=new Number(-0);
    array[item++] = new TestCase( SECTION,    "o=-0;o.toLocaleString()",               "0",                   o.toLocaleString() );

    o=new Number(Number.POSITIVE_INFINITY);
    array[item++] = new TestCase( SECTION,    "o=new Number(Number.POSITIVE_INFINITY)", "Infinity",     o.toLocaleString() );
    o=new Number(Number.NEGATIVE_INFINITY);
    array[item++] = new TestCase( SECTION,    "o=new Number(Number.NEGATIVE_INFINITY);o.toLocaleString()", "-Infinity",     o.toLocaleString() );
  
    o=new Number(-1);
    array[item++] = new TestCase( SECTION,    "o=new Number(-1);o.toLocaleString()", "-1",     o.toLocaleString() );

    // cases in step 6:  integers  1e21 > x >= 1 or -1 >= x > -1e21
   
    o=new Number(1);
    array[item++] = new TestCase( SECTION,    "o=new Number(1);o.toLocaleString()", "1",     o.toLocaleString() );
    o=new Number(10);
    array[item++] = new TestCase( SECTION,    "o=new Number(10);o.toLocaleString()", "10",     o.toLocaleString() );
    o=new Number(100);
    array[item++] = new TestCase( SECTION,    "o=new Number(100);o.toLocaleString()", "100",     o.toLocaleString() );
    o=new Number(1000);
    array[item++] = new TestCase( SECTION,    "o=new Number(1000);o.toLocaleString()", "1000",     o.toLocaleString() );
    o=new Number(10000);
    array[item++] = new TestCase( SECTION,    "o=new Number(10000);o.toLocaleString()", "10000",     o.toLocaleString() );
    o=new Number(10000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(10000000000);o.toLocaleString()", "10000000000",o.toLocaleString() );
    o=new Number(10000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(10000000000000000000);o.toString()", "10000000000000000000",o.toString() );
    o=new Number(100000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(100000000000000000000);o.toLocaleString()", "100000000000000000000",o.toLocaleString() );
    o=new Number(12345 );
    array[item++] = new TestCase( SECTION,    "o=new Number(12345 );o.toLocaleString()", "12345",o.toLocaleString() );
    o=new Number(1234567890 );
    array[item++] = new TestCase( SECTION,    "o=new Number(1234567890);o.toLocaleString()", "1234567890",o.toLocaleString() );
    o=new Number(-1);
    array[item++] = new TestCase( SECTION,    "o=new Number(-1);o.toLocaleString()", "-1",o.toLocaleString() );
    o=new Number(-10 );
    array[item++] = new TestCase( SECTION,    "o=new Number(-10 );o.toLocaleString()", "-10",o.toLocaleString() );
   
    o=new Number(-100 );
    array[item++] = new TestCase( SECTION,    "o=new Number(-100 );o.toLocaleString()", "-100",o.toLocaleString() );
    o=new Number(-1000 );
    array[item++] = new TestCase( SECTION,    "o=new Number(-1000 );o.toLocaleString()", "-1000",o.toLocaleString() ); 
    o=new Number(-1000000000 );
    array[item++] = new TestCase( SECTION,    "o=new Number(-1000000000 );o.toLocaleString()", "-1000000000",o.toLocaleString() );
    o=new Number(-1000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(-1000000000000000);o.toLocaleString()", "-1000000000000000",o.toLocaleString() );
    o=new Number(-100000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(-100000000000000000000);o.toLocaleString()", "-100000000000000000000",o.toLocaleString() );

    o=new Number(-1000000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(-1000000000000000000000);o.toLocaleString()", "-1e+21",o.toLocaleString() );

    o=new Number(1.0000001 );
    array[item++] = new TestCase( SECTION,    "o=new Number(1.0000001);o.toLocaleString()", "1.0000001",o.toLocaleString() );
    o=new Number(1000000000000000000000);
    array[item++] = new TestCase( SECTION,    "o=new Number(1000000000000000000000);o.toLocaleString()", "1e+21",o.toLocaleString() );

    o=new Number(1.2345);
    array[item++] = new TestCase( SECTION,    "o=new Number(1.2345);o.toLocaleString()", "1.2345",o.toLocaleString() );

    o=new Number(1.234567890);
    array[item++] = new TestCase( SECTION,    "o=new Number(1.234567890);o.toLocaleString()", "1.23456789",o.toLocaleString() );

    o=new Number(.12345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.12345);o.toLocaleString()", "0.12345",o.toLocaleString() );

    o=new Number(.012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.012345);o.toLocaleString()", "0.012345",o.toLocaleString() );
    
    o=new Number(.0012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.0012345);o.toLocaleString()", "0.0012345",o.toLocaleString() );
    o=new Number(.00012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.00012345);o.toLocaleString()", "0.00012345",o.toLocaleString() );
    o=new Number(.000012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.000012345);o.toLocaleString()", "0.000012345",o.toLocaleString() );
    o=new Number(.0000012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.0000012345);o.toLocaleString()", "0.0000012345",o.toLocaleString() );
    o=new Number(.00000012345);
    array[item++] = new TestCase( SECTION,    "o=new Number(.00000012345);o.toLocaleString()", "1.2345e-7",o.toLocaleString() );
    o=new Number();
    array[item++] = new TestCase( SECTION,    "o=new Number();o.toLocaleString()", "0",o.toLocaleString() );
      

    return ( array );
}
