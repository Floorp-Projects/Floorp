/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.1.js
   ECMA Section:       15.5.1 The String Constructor called as a Function
   15.5.1.1 String(value)
   15.5.1.2 String()

   Description:	When String is called as a function rather than as
   a constructor, it performs a type conversion.
   - String(value) returns a string value (not a String
   object) computed by ToString(value)
   - String() returns the empty string ""

   Author:             christine@netscape.com
   Date:               1 october 1997
*/

var SECTION = "15.5.1";
var TITLE   = "The String Constructor Called as a Function";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "String('string primitive')",	"string primitive",	String('string primitive') );
new TestCase( "String(void 0)",		"undefined",		String( void 0) );
new TestCase( "String(null)",			    "null",			String( null ) );
new TestCase( "String(true)",			    "true",			String( true) );
new TestCase( "String(false)",		    "false",		String( false ) );
new TestCase( "String(Boolean(true))",	"true",			String(Boolean(true)) );
new TestCase( "String(Boolean(false))",	"false",		String(Boolean(false)) );
new TestCase( "String(Boolean())",		"false",		String(Boolean(false)) );
new TestCase( "String(new Array())",		"",			    String( new Array()) );
new TestCase( "String(new Array(1,2,3))",	"1,2,3",		String( new Array(1,2,3)) );


new TestCase( "String( Number.NaN )",       "NaN",                  String( Number.NaN ) );
new TestCase( "String( 0 )",                "0",                    String( 0 ) );
new TestCase( "String( -0 )",               "0",                   String( -0 ) );
new TestCase( "String( Number.POSITIVE_INFINITY )", "Infinity",     String( Number.POSITIVE_INFINITY ) );
new TestCase( "String( Number.NEGATIVE_INFINITY )", "-Infinity",    String( Number.NEGATIVE_INFINITY ) );
new TestCase( "String( -1 )",               "-1",                   String( -1 ) );

// cases in step 6:  integers  1e21 > x >= 1 or -1 >= x > -1e21

new TestCase( "String( 1 )",                    "1",                    String( 1 ) );
new TestCase( "String( 10 )",                   "10",                   String( 10 ) );
new TestCase( "String( 100 )",                  "100",                  String( 100 ) );
new TestCase( "String( 1000 )",                 "1000",                 String( 1000 ) );
new TestCase( "String( 10000 )",                "10000",                String( 10000 ) );
new TestCase( "String( 10000000000 )",          "10000000000",          String( 10000000000 ) );
new TestCase( "String( 10000000000000000000 )", "10000000000000000000", String( 10000000000000000000 ) );
new TestCase( "String( 100000000000000000000 )","100000000000000000000",String( 100000000000000000000 ) );

new TestCase( "String( 12345 )",                    "12345",                    String( 12345 ) );
new TestCase( "String( 1234567890 )",               "1234567890",               String( 1234567890 ) );

new TestCase( "String( -1 )",                       "-1",                       String( -1 ) );
new TestCase( "String( -10 )",                      "-10",                      String( -10 ) );
new TestCase( "String( -100 )",                     "-100",                     String( -100 ) );
new TestCase( "String( -1000 )",                    "-1000",                    String( -1000 ) );
new TestCase( "String( -1000000000 )",              "-1000000000",              String( -1000000000 ) );
new TestCase( "String( -1000000000000000 )",        "-1000000000000000",        String( -1000000000000000 ) );
new TestCase( "String( -100000000000000000000 )",   "-100000000000000000000",   String( -100000000000000000000 ) );
new TestCase( "String( -1000000000000000000000 )",  "-1e+21",                   String( -1000000000000000000000 ) );

new TestCase( "String( -12345 )",                    "-12345",                  String( -12345 ) );
new TestCase( "String( -1234567890 )",               "-1234567890",             String( -1234567890 ) );

// cases in step 7: numbers with a fractional component, 1e21> x >1 or  -1 > x > -1e21,
new TestCase( "String( 1.0000001 )",                "1.0000001",                String( 1.0000001 ) );


// cases in step 8:  fractions between 1 > x > -1, exclusive of 0 and -0

// cases in step 9:  numbers with 1 significant digit >= 1e+21 or <= 1e-6

new TestCase( "String( 1000000000000000000000 )",   "1e+21",             String( 1000000000000000000000 ) );
new TestCase( "String( 10000000000000000000000 )",   "1e+22",            String( 10000000000000000000000 ) );

//  cases in step 10:  numbers with more than 1 significant digit >= 1e+21 or <= 1e-6
new TestCase( "String( 1.2345 )",                    "1.2345",                  String( 1.2345));
new TestCase( "String( 1.234567890 )",               "1.23456789",             String( 1.234567890 ));

new TestCase( "String( .12345 )",                   "0.12345",               String(.12345 )     );
new TestCase( "String( .012345 )",                  "0.012345",              String(.012345)     );
new TestCase( "String( .0012345 )",                 "0.0012345",             String(.0012345)    );
new TestCase( "String( .00012345 )",                "0.00012345",            String(.00012345)   );
new TestCase( "String( .000012345 )",               "0.000012345",           String(.000012345)  );
new TestCase( "String( .0000012345 )",              "0.0000012345",          String(.0000012345) );
new TestCase( "String( .00000012345 )",             "1.2345e-7",            String(.00000012345));

new TestCase( "String()",			        "",			    String() );

test();
