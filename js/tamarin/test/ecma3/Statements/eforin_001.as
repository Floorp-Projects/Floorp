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
    var SECTION = "forin-001";
    var VERSION = "ECMA_2";
    var TITLE   = "The for...in  statement";
    var BUGNUMBER="330890";
    var BUGNUMBER="http://scopus.mcom.com/bugsplat/show_bug.cgi?id=344855";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var tc = 0;
    var testcases = new Array();

    ForIn_1( { length:4, company:"netscape", year:2000, 0:"zero" } );
    ForIn_2( { length:4, company:"netscape", year:2000, 0:"zero" } );
    ForIn_3( { length:4, company:"netscape", year:2000, 0:"zero" } );

//    ForIn_6({ length:4, company:"netscape", year:2000, 0:"zero" });
//    ForIn_7({ length:4, company:"netscape", year:2000, 0:"zero" });
    ForIn_8({ length:4, company:"netscape", year:2000, 0:"zero" });

    test();

    function ForIn_1( object ) {
        PropertyArray = new Array();
        ValueArray = new Array();

        for ( PropertyArray[PropertyArray.length] in object ) {
            ValueArray[ValueArray.length] =
                object[PropertyArray[PropertyArray.length-1]];
            
        }

		tcCompany = tc+0;
		tcLength = tc+1;
		tcZero = tc+2;
		tcYear = tc+3;

		// need a hack to make sure that the order of test cases
		// is constant... ecma stats that the order that for-in
		// is run does not have to be constant 
        for ( var i = 0; i < PropertyArray.length; i++ ) {
                        
			switch( PropertyArray[i] ) {
				case "company":
            		testcases[tcCompany] = new TestCase(
                		SECTION,
                		"object[" + PropertyArray[i] +"]",
                		object[PropertyArray[i]],
                		ValueArray[i]
            		);
					tc++;
					break;

				case "length":
            		testcases[tcLength] = new TestCase(
                		SECTION,
                		"object[" + PropertyArray[i] +"]",
                		object[PropertyArray[i]],
                		ValueArray[i]
            		);
					tc++;
					break;

				case "year":
            		testcases[tcYear] = new TestCase(
                		SECTION,
                		"object[" + PropertyArray[i] +"]",
                		object[PropertyArray[i]],
                		ValueArray[i]
            		);
					tc++;
					break;

				case "0":
            		testcases[tcZero] = new TestCase(
                		SECTION,
                		"object[" + PropertyArray[i] +"]",
                		object[PropertyArray[i]],
                		ValueArray[i]
            		);
					tc++;
					break;

			}
        }

        testcases[tc++] = new TestCase(
            SECTION,
            "object.length",
            PropertyArray.length,
            object.length );
    }

    function ForIn_2( object ) {
        PropertyArray = new Array();
        ValueArray = new Array();
        var i = 0;

        for ( PropertyArray[i++] in object ) {
            ValueArray[ValueArray.length] =
                object[PropertyArray[PropertyArray.length-1]];
        }

		tcCompany = tc+0;
		tcLength = tc+1;
		tcZero = tc+2;
		tcYear = tc+3;

		// need a hack to make sure that the order of test cases
		// is constant... ecma stats that the order that for-in
		// is run does not have to be constant 
        for ( var i = 0; i < PropertyArray.length; i++ ) {
			switch( PropertyArray[i] ) {
				case "company":
            		testcases[tcCompany] = new TestCase(
                		SECTION,
                		"object[" + PropertyArray[i] +"]",
                		object[PropertyArray[i]],
                		ValueArray[i]
            		);
					tc++
					break;

				case "length":
            		testcases[tcLength] = new TestCase(
                		SECTION,
                		"object[" + PropertyArray[i] +"]",
                		object[PropertyArray[i]],
                		ValueArray[i]
            		);
					tc++
					break;

				case "year":
            		testcases[tcYear] = new TestCase(
                		SECTION,
                		"object[" + PropertyArray[i] +"]",
                		object[PropertyArray[i]],
                		ValueArray[i]
            		);
					tc++
					break;

				case "0":
            		testcases[tcZero] = new TestCase(
                		SECTION,
                		"object[" + PropertyArray[i] +"]",
                		object[PropertyArray[i]],
                		ValueArray[i]
            		);
					tc++
					break;

			}
        }

        testcases[tc++] = new TestCase(
            SECTION,
            "object.length",
            PropertyArray.length,
            object.length );
    }

    function ForIn_3( object ) {
        var checkBreak = "pass";
        var properties = new Array();
        var values = new Array();

        for ( properties[properties.length] in object ) {
            values[values.length] = object[properties[properties.length-1]];
            break;
            checkBreak = "fail";
        }

        testcases[tc++] = new TestCase(
            SECTION,
            "check break out of for...in",
            "pass",
            checkBreak );

        testcases[tc++] = new TestCase(
            SECTION,
            "properties.length",
            1,
            properties.length );

		// we don't know which one of the properties
		// because we can't predict order
		var myTest = "PASSED";
		if( values[0] != object[properties[0]] ) 
			myTest = "FAILED";

        testcases[tc++] = new TestCase(
            SECTION,
            "object[properties[0]] == values[0]",
            "PASSED",
            myTest );
    }

    function ForIn_4( object ) {
        var result1 = 0;
        var result2 = 0;
        var result3 = 0;
        var result4 = 0;
        var i = 0;
        var property = new Array();

        butterbean: {
            result1++;

            for ( property[i++] in object ) {
                result2++;
                break;
                result4++;
            }
            result3++;
        }

        testcases[tc++] = new TestCase(
            SECTION,
            "verify labeled statement is only executed once",
            true,
            result1 == 1 );

        testcases[tc++] = new TestCase(
            SECTION,
            "verify statements in for loop are evaluated",
            true,
            result2 == i );

        testcases[tc++] = new TestCase(
            SECTION,
            "verify break out of labeled for...in loop",
            true,
            result4 == 0 );

        testcases[tc++] = new TestCase(
            SECTION,
            "verify break out of labeled block",
            true,
            result3 == 0 );
    }

    function ForIn_5 (object) {
        var result1 = 0;
        var result2 = 0;
        var result3 = 0;
        var result4 = 0;
        var i = 0;
        var property = new Array();

        bigredbird: {
            result1++;
            for ( property[i++] in object ) {
                result2++;
                break bigredbird;
                result4++;
            }
            result3++;
        }

        testcases[tc++] = new TestCase(
            SECTION,
            "verify labeled statement is only executed once",
            true,
            result1 == 1 );

        testcases[tc++] = new TestCase(
            SECTION,
            "verify statements in for loop are evaluated",
            true,
            result2 == i );

        testcases[tc++] = new TestCase(
            SECTION,
            "verify break out of labeled for...in loop",
            true,
            result4 == 0 );

        testcases[tc++] = new TestCase(
            SECTION,
            "verify break out of labeled block",
            true,
            result3 == 0 );
    }

    function ForIn_7( object ) {
        var result1 = 0;
        var result2 = 0;
        var result3 = 0;
        var result4 = 0;
        var i = 0;
        var property = new Array();

        bigredbird:
            for ( property[i++] in object ) {
                result2++;
                continue bigredbird;
                result4++;
            }

        testcases[tc++] = new TestCase(
            SECTION,
            "verify statements in for loop are evaluated",
            true,
            result2 == i );

        testcases[tc++] = new TestCase(
            SECTION,
            "verify break out of labeled for...in loop",
            true,
            result4 == 0 );

        testcases[tc++] = new TestCase(
            SECTION,
            "verify break out of labeled block",
            true,
            result3 == 1 );
    }


    function ForIn_8( object ) {
        var checkBreak = "pass";
        var properties = new Array();
        var values = new Array();

        for ( properties[properties.length] in object ) {
            values[values.length] = object[properties[properties.length-1]];
            break;
            checkBreak = "fail";
        }

        testcases[tc++] = new TestCase(
            SECTION,
            "check break out of for...in",
            "pass",
            checkBreak );

        testcases[tc++] = new TestCase(
            SECTION,
            "properties.length",
            1,
            properties.length );

		// we don't know which one of the properties
		// because we can't predict order
		var myTest = "PASSED";
		if( values[0] != object[properties[0]] ) 
			myTest = "FAILED";


        testcases[tc++] = new TestCase(
            SECTION,
            "object[properties[0]] == object[properties[0]]",
            "PASSED",
            myTest );
    }

