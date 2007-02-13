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
	
    var SECTION = "15.5.2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The String Constructor";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

	String.prototype.myToString=Object.prototype.toString;
	TESTSTRING = new String('string primitive'); 
	
    array[item++] = new TestCase( SECTION,	"typeof new String('string primitive')",	    "string",	        typeof new String('string primitive') );
    array[item++] = new TestCase( SECTION,	"var TESTSTRING = new String('string primitive'), TESTSTRING.myToString=Object.prototype.toString,TESTSTRING.myToString()", "[object String]",   TESTSTRING.myToString() );
    array[item++] = new TestCase( SECTION,  "(new String('string primitive')).valueOf()",   'string primitive', (new String('string primitive')).valueOf() );
    array[item++] = new TestCase( SECTION,  "(new String('string primitive')).substring",   "function Function() {}",((new String('string primitive')).substring+"") );

    array[item++] = new TestCase( SECTION,	"typeof new String(void 0)",	                "string",	        typeof new String(void 0) );
    array[item++] = new TestCase( SECTION,	"var TESTSTRING = new String(void 0), TESTSTRING.myToString=Object.prototype.toString,TESTSTRING.myToString()", "[object String]",   (TESTSTRING = new String(void 0), TESTSTRING.myToString() ) );
    array[item++] = new TestCase( SECTION,  "(new String(void 0)).valueOf()",               "undefined", (new String(void 0)).valueOf() );
    array[item++] = new TestCase( SECTION,  "(new String(void 0)).toString",               "function Function() {}",((new String(void 0)).toString+"") );

    array[item++] = new TestCase( SECTION,	"typeof new String(null)",	            "string",	        typeof new String(null) );
    array[item++] = new TestCase( SECTION,	"var TESTSTRING = new String(null), TESTSTRING.myToString=Object.prototype.toString,TESTSTRING.myToString()", "[object String]",   (TESTSTRING = new String(null), TESTSTRING.myToString() ) );
    array[item++] = new TestCase( SECTION,  "(new String(null)).valueOf()",         "null",             (new String(null)).valueOf() );
    array[item++] = new TestCase( SECTION,  "(new String(null)).valueOf",         (String.prototype.valueOf+""),  ((new String(null)).valueOf+"") );

    array[item++] = new TestCase( SECTION,	"typeof new String(true)",	            "string",	        typeof new String(true) );
    array[item++] = new TestCase( SECTION,	"var TESTSTRING = new String(true), TESTSTRING.myToString=Object.prototype.toString,TESTSTRING.myToString()", "[object String]",   (TESTSTRING = new String(true), TESTSTRING.myToString() ) );
    array[item++] = new TestCase( SECTION,  "(new String(true)).valueOf()",         "true",             (new String(true)).valueOf() );
    array[item++] = new TestCase( SECTION,  "(new String(true)).charAt",         (String.prototype.charAt+""),((new String(true)).charAt+"") );

    array[item++] = new TestCase( SECTION,	"typeof new String(false)",	            "string",	        typeof new String(false) );
    array[item++] = new TestCase( SECTION,	"var TESTSTRING = new String(false), TESTSTRING.myToString=Object.prototype.toString,TESTSTRING.myToString()", "[object String]",   (TESTSTRING = new String(false), TESTSTRING.myToString() ) );
    array[item++] = new TestCase( SECTION,  "(new String(false)).valueOf()",        "false",            (new String(false)).valueOf() );
    array[item++] = new TestCase( SECTION,  "(new String(false)).charCodeAt",        (String.prototype.charCodeAt+""),   ((new String(false)).charCodeAt+"") );

    array[item++] = new TestCase( SECTION,	"typeof new String(new Boolean(true))",	       "string",	        typeof new String(new Boolean(true)) );
    array[item++] = new TestCase( SECTION,	"var TESTSTRING = new String(new Boolean(true)), TESTSTRING.toString=Object.prototype.toString,TESTSTRING.toString()", "true",   (TESTSTRING = new String(new Boolean(true)), TESTSTRING.toString() ) );
    array[item++] = new TestCase( SECTION,  "(new String(new Boolean(true))).valueOf()",   "true",              (new String(new Boolean(true))).valueOf() );
    array[item++] = new TestCase( SECTION,  "(new String(new Boolean(true))).indexOf",   (String.prototype.indexOf+""),((new String(new Boolean(true))).indexOf+"") );

    array[item++] = new TestCase( SECTION,	"typeof new String()",	                        "string",	        typeof new String() );
    array[item++] = new TestCase( SECTION,	"var TESTSTRING = new String(), TESTSTRING.myToString=Object.prototype.toString,TESTSTRING.myToString()", "[object String]",   (TESTSTRING = new String(), TESTSTRING.myToString() ) );
    array[item++] = new TestCase( SECTION,  "(new String()).valueOf()",   '',                 (new String()).valueOf() );
    array[item++] = new TestCase( SECTION,  "(new String()).lastIndexOf",   (String.prototype.lastIndexOf+""),((new String()).lastIndexOf+"") );

    array[item++] = new TestCase( SECTION,	"typeof new String('')",	    "string",	        typeof new String('') );
    array[item++] = new TestCase( SECTION,	"var TESTSTRING = new String(''), TESTSTRING.myToString=Object.prototype.toString,TESTSTRING.myToString()", "[object String]",   (TESTSTRING = new String(''), TESTSTRING.myToString() ) );
    array[item++] = new TestCase( SECTION,  "(new String('')).valueOf()",   '',                 (new String('')).valueOf() );
    array[item++] = new TestCase( SECTION,  "(new String('')).split",   (String.prototype.split+""),((new String('')).split+"") );
    array[item++] = new TestCase( SECTION,	"new String(true)",			    "true",new String( true) );
    array[item++] = new TestCase( SECTION,	"new String(false)",		    "false",		new String( false ) );
    array[item++] = new TestCase( SECTION,	"new String(new Array())",		"",			    new String( new Array()) );
    array[item++] = new TestCase( SECTION,	"new String(new Array(1,2,3))",	"1,2,3",		new String( new Array(1,2,3)) );
    array[item++] = new TestCase( SECTION,    "new String( Number.NaN )",       "NaN",                  new String( Number.NaN ) );
    array[item++] = new TestCase( SECTION,    "new String( 0 )",                "0",                    new String( 0 ) );
    array[item++] = new TestCase( SECTION,    "new String( -0 )",               "0",                   new String( -0 ) );
    array[item++] = new TestCase( SECTION,    "new String( Number.POSITIVE_INFINITY )", "Infinity",     new String( Number.POSITIVE_INFINITY ) );
    array[item++] = new TestCase( SECTION,    "new String( Number.NEGATIVE_INFINITY )", "-Infinity",    new String( Number.NEGATIVE_INFINITY ) );
    array[item++] = new TestCase( SECTION,    "new String( -1 )",               "-1",                   new String( -1 ) );

    // cases in step 6:  integers  1e21 > x >= 1 or -1 >= x > -1e21

    array[item++] = new TestCase( SECTION,    "new String( 1 )",                    "1",                    new String( 1 ) );
    array[item++] = new TestCase( SECTION,    "new String( 10 )",                   "10",                   new String( 10 ) );
    array[item++] = new TestCase( SECTION,    "new String( 100 )",                  "100",                  new String( 100 ) );
    array[item++] = new TestCase( SECTION,    "new String( 1000 )",                 "1000",                 new String( 1000 ) );
    array[item++] = new TestCase( SECTION,    "new String( 10000 )",                "10000",                new String( 10000 ) );
    array[item++] = new TestCase( SECTION,    "new String( 10000000000 )",          "10000000000",new String( 10000000000 ) );
    array[item++] = new TestCase( SECTION,    "new String( 10000000000000000000 )", "10000000000000000000", new String( 10000000000000000000 ) );
    array[item++] = new TestCase( SECTION,    "new String( 100000000000000000000 )","100000000000000000000",new String( 100000000000000000000 ) );

    array[item++] = new TestCase( SECTION,    "new String( 12345 )",                    "12345",                    new String( 12345 ) );
    array[item++] = new TestCase( SECTION,    "new String( 1234567890 )",               "1234567890",               new String( 1234567890) );

    array[item++] = new TestCase( SECTION,    "new String( -1 )",                       "-1",                       new String( -1 ) );
    array[item++] = new TestCase( SECTION,    "new String( -10 )",                      "-10",                      new String( -10 ) );
    array[item++] = new TestCase( SECTION,    "new String( -100 )",                     "-100",                     new String( -100 ) );
    array[item++] = new TestCase( SECTION,    "new String( -1000 )",                    "-1000",                    new String( -1000 ) );
    array[item++] = new TestCase( SECTION,    "new String( -1000000000 )",              "-1000000000",              new String( -1000000000 ) );
    array[item++] = new TestCase( SECTION,    "new String( -1000000000000000 )",        "-1000000000000000",        new String( -1000000000000000 ) );
    array[item++] = new TestCase( SECTION,    "new String( -100000000000000000000 )",   "-100000000000000000000",  new String( -100000000000000000000 ) );
    array[item++] = new TestCase( SECTION,    "new String( -1000000000000000000000 )",  "-1e+21",                   new String( -1000000000000000000000 ) );

    array[item++] = new TestCase( SECTION,    "new String( -12345 )",                    "-12345",                  new String( -12345 ) );
    array[item++] = new TestCase( SECTION,    "new String( -1234567890 )",               "-1234567890",new String( -1234567890 ) );

    // cases in step 7: numbers with a fractional component, 1e21> x >1 or  -1 > x > -1e21,
    array[item++] = new TestCase( SECTION,    "new String( 1.0000001 )",                "1.0000001",new String( 1.0000001 ) );


    // cases in step 8:  fractions between 1 > x > -1, exclusive of 0 and -0

    // cases in step 9:  numbers with 1 significant digit >= 1e+21 or <= 1e-6

    array[item++] = new TestCase( SECTION,    "new String( 1000000000000000000000 )",   "1e+21", new String( 1000000000000000000000 ) );
    array[item++] = new TestCase( SECTION,    "new String( 10000000000000000000000 )",   "1e+22",new String( 10000000000000000000000 ) );

    //  cases in step 10:  numbers with more than 1 significant digit >= 1e+21 or <= 1e-6
    array[item++] = new TestCase( SECTION,    "new String( 1.2345 )",                    "1.2345",new String( 1.2345));
    array[item++] = new TestCase( SECTION,    "new String( 1.234567890 )",               "1.23456789",new String( 1.234567890 ));

    array[item++] = new TestCase( SECTION,    "new String( .12345 )",                   "0.12345",new String(.12345 )     );
    array[item++] = new TestCase( SECTION,    "new String( .012345 )",                  "0.012345",new String(.012345)     );
    array[item++] = new TestCase( SECTION,    "new String( .0012345 )",                 "0.0012345",new String(.0012345)    );
    array[item++] = new TestCase( SECTION,    "new String( .00012345 )",                "0.00012345",new String(.00012345)   );
    array[item++] = new TestCase( SECTION,    "new String( .000012345 )",               "0.000012345",new String(.000012345)  );
    array[item++] = new TestCase( SECTION,    "new String( .0000012345 )",              "0.0000012345",new String(.0000012345) );
    array[item++] = new TestCase( SECTION,    "new String( .00000012345 )",             "1.2345e-7",new String(.00000012345));

    array[item++] = new TestCase( "15.5.2",	"new String()","",new String() );

    return ( array );
}

