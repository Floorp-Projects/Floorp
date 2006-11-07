/* ***** BEGIN LICENSE BLOCK ***** 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1 

The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
"License"); you may not use this file except in compliance with the License. You may obtain 
a copy of the License at http://www.mozilla.org/MPL/ 

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
language governing rights and limitations under the License. 

The Original Code is [Open Source Virtual Machine.] 

The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
by the Initial Developer are Copyright (C)[ 2005-2006 ] Adobe Systems Incorporated. All Rights 
Reserved. 

Contributor(s): Adobe AS3 Team

Alternatively, the contents of this file may be used under the terms of either the GNU 
General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
LGPL are applicable instead of those above. If you wish to allow use of your version of this 
file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
version of this file under the terms of the MPL, indicate your decision by deleting provisions 
above and replace them with the notice and other provisions required by the GPL or the 
LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
under the terms of any one of the MPL, the GPL or the LGPL. 

 ***** END LICENSE BLOCK ***** */

    var SECTION = "8.4";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The Number type";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;
    
    //NaN values of ECMAScript

    var s:Number = 'string'
    array[item++] = new TestCase( SECTION,
                                    "var s:Number = 'string'",
                                    NaN,s );
    var k:Number = 'string'

    array[item++] = new TestCase( SECTION,
                                    "Two NaN values are different from each other",
                                    true,
                                    (k!=s) );

    //Positive Infinity and Negative Infinity

    array[item++] = new TestCase( SECTION,
                                    "The special value positive Infinity",
                                    Infinity,
                                    Number.POSITIVE_INFINITY );

    array[item++] = new TestCase( SECTION,
                                    "The special value negative Infinity",
                                    -Infinity,
                                    Number.NEGATIVE_INFINITY );

    //Positive zero and Negative Zero

    var x:Number = +0;

    array[item++] = new TestCase( SECTION,
                                    "positive zero",
                                    +0,
                                     x );

    array[item++] = new TestCase( SECTION,
                                    "positive zero",
                                    true,
                                     x==0 );

    var y:Number = -0;

    array[item++] = new TestCase( SECTION,
                                    "Negative zero",
                                    -0,
                                     y );


    array[item++] = new TestCase( SECTION,
                                    "Negative zero",
                                    true,
                                     y==0 );

    array[item++] = new TestCase( SECTION,
                                    "Negative zero==Positive zero",
                                    true,
                                     y==x );

    //Finite Non-zero values

    //Finite nonzero values  that are Normalised having the form s*m*2**e
    // where s is +1 or -1, m is a positive integer less than 2**53 but not
    // less than s**52 and e is an integer ranging from -1074 to 971

    array[item++] = new TestCase( SECTION,"Positive finite Non zero values where e is -1074",4.4501477170144023e-308,(1*((Math.pow(2,53))-1)*(Math.pow(2,-1074))) );


    array[item++] = new TestCase( SECTION,"Positive finite Non zero values where e is -1074",2.2250738585072014e-308,(1*(Math.pow(2,52))*(Math.pow(2,-1074))) );

    array[item++] = new TestCase( SECTION,"Positive finite Non zero values where e is 971", "1e+308",(1*(Math.pow(2,52))*(Math.pow(2,971)))+"" );

    array[item++] = new TestCase( SECTION,"Positive finite Non zero values where e is 971", "1.79769313486231e+308",(1*((Math.pow(2,53))-1)*(Math.pow(2,971)))+"" );

    array[item++] = new TestCase( SECTION,"Negative finite Non zero values where e is -1074",-2.2250738585072014e-308,(-1*(Math.pow(2,52))*(Math.pow(2,-1074))) );

    array[item++] = new TestCase( SECTION,"Negative finite Non zero values where e is 971", "-e+308",(-1*(Math.pow(2,52))*(Math.pow(2,971)))+"" );


    //Finite nonzero values  that are denormalised having the form s*m*2**e
    // where s is +1 or -1, m is a positive integer less than 2**52
    // and e is -1074

    array[item++] = new TestCase( SECTION,"Positive finite Non zero values where e is -1074",1.1125369292536007e-308,(1*(Math.pow(2,51))*(Math.pow(2,-1074))) );

    array[item++] = new TestCase( SECTION,"Positive finite Non zero values where e is -1074",-1.1125369292536007e-308,(-1*(Math.pow(2,51))*(Math.pow(2,-1074))) );

    //When number value for Y is closest to 2**1024 should convert to Infinity where x is
    // nonzero real mathematical quantity

    var Y:Number = 1e+308*2

    array[item++] = new TestCase( SECTION,"the number value for x closest to 2**1024",Infinity,Y);

    array[item++] = new TestCase( SECTION,"the number value for x closest to 2**1024",Infinity,(1*(Math.pow(2,53))*(Math.pow(2,971))));



    //When number value for z is closest to -2**1024 should convert to Infinity where x is
    // nonzero real mathematical quantity

    var z:Number = -1e+308*3

    array[item++] = new TestCase( SECTION,"the number value for x closest to -2**1024",-Infinity,z);

    //When number value for l is closest to +0 should convert to -0 where l is
    // nonzero real mathematical quantity and less than 0
    // Expected result should be -0, however, base 10 approximations of a base 2 number results in loss of precision

    var l:Number = 1e-308*2

    array[item++] = new TestCase( SECTION,"the number value for x closest to +0",1.9999999999999998e-308,l);

    var m:Number = -1e-308*3

    array[item++] = new TestCase( SECTION,"the number value for x closest to +0",-2.9999999999999997e-308,m);
    return array;
}
