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
    var SECTION = "15.5.4.7-2";
    var VERSION = "ECMA_4";
    startTest();
    var TITLE   = "String.protoype.lastIndexOf";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();


function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION, "String.prototype.lastIndexOf.length",           2,          String.prototype.lastIndexOf.length );
    array[item++] = new TestCase( SECTION, "delete String.prototype.lastIndexOf.length",    false,      delete String.prototype.lastIndexOf.length );
    array[item++] = new TestCase( SECTION, "delete String.prototype.lastIndexOf.length; String.prototype.lastIndexOf.length",  2,  (delete String.prototype.lastIndexOf.length, String.prototype.lastIndexOf.length ) );

    array[item++] = new TestCase( SECTION, "var s = new String(''), s.lastIndexOf('', 0)",          LastIndexOf("","",0),  (s = new String(''), s.lastIndexOf('', 0) ) );
    array[item++] = new TestCase( SECTION, "var s = new String(''), s.lastIndexOf('')",             LastIndexOf("",""),    (s = new String(''), s.lastIndexOf('') ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('', 0)",     LastIndexOf("hello","",0),  (s = new String('hello'), s.lastIndexOf('',0) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('')",        LastIndexOf("hello",""),    (s = new String('hello'), s.lastIndexOf('') ) );

    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll')",     LastIndexOf("hello","ll" ),   (s = new String('hello'), s.lastIndexOf('ll') ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', 0)",  LastIndexOf("hello","ll",0),  (s = new String('hello'), s.lastIndexOf('ll', 0) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', 1)",  LastIndexOf("hello","ll",1),  (s = new String('hello'), s.lastIndexOf('ll', 1) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', 2)",  LastIndexOf("hello","ll",2),  (s = new String('hello'), s.lastIndexOf('ll', 2) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', 3)",  LastIndexOf("hello","ll",3),  (s = new String('hello'), s.lastIndexOf('ll', 3) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', 4)",  LastIndexOf("hello","ll",4),  (s = new String('hello'), s.lastIndexOf('ll', 4) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', 5)",  LastIndexOf("hello","ll",5),  (s = new String('hello'), s.lastIndexOf('ll', 5) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', 6)",  LastIndexOf("hello","ll",6),  (s = new String('hello'), s.lastIndexOf('ll', 6) ) );

    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', 1.5)", LastIndexOf('hello','ll', 1.5), (s = new String('hello'), s.lastIndexOf('ll', 1.5) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', 2.5)", LastIndexOf('hello','ll', 2.5),  (s = new String('hello'), s.lastIndexOf('ll', 2.5) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', -1)",  LastIndexOf('hello','ll', -1), (s = new String('hello'), s.lastIndexOf('ll', -1) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', -1.5)",LastIndexOf('hello','ll', -1.5), (s = new String('hello'), s.lastIndexOf('ll', -1.5) ) );


    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', -Infinity)",    LastIndexOf("hello","ll",-Infinity), (s = new String('hello'), s.lastIndexOf('ll', -Infinity) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', Infinity)",    LastIndexOf("hello","ll",Infinity), (s = new String('hello'), s.lastIndexOf('ll', Infinity) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', NaN)",    LastIndexOf("hello","ll",NaN), (s = new String('hello'), s.lastIndexOf('ll', NaN) ) );
    array[item++] = new TestCase( SECTION, "var s = new String('hello'), s.lastIndexOf('ll', -0)",    LastIndexOf("hello","ll",-0), (s = new String('hello'), s.lastIndexOf('ll', -0) ) );

    for ( var i = 0; i < ( "[object Object]" ).length; i++ ) {
        array[item++] = new TestCase(   SECTION,
                                        "var o = new Object(); o.lastIndexOf = String.prototype.lastIndexOf; o.lastIndexOf('b', "+ i + ")",
                                        ( i < 2 ? -1 : ( i < 9  ? 2 : 9 )) ,
                                        (o = new Object(), o.lastIndexOf = String.prototype.lastIndexOf, o.lastIndexOf('b', i )) );
    }

    var origBooleanLastIndexOf = Boolean.prototype.lastIndexOf;
	Boolean.prototype.lastIndexOf = String.prototype.lastIndexOf;
    for ( var i = 0; i < 5; i ++ ) {
        array[item++] = new TestCase(   SECTION,
                                        "var b = new Boolean(); b.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('l', "+ i + ")",
                                        ( i < 2 ? -1 : 2 ),
                                        (b = new Boolean(), b.lastIndexOf('l', i )) );
    }

    var origBooleanToString = Boolean.prototype.toString;
	Boolean.prototype.toString=Object.prototype.toString;
    for ( var i = 0; i < 5; i ++ ) {
        array[item++] = new TestCase(   SECTION,
                                        "var b = new Boolean(); b.toString = Object.prototype.toString; b.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('o', "+ i + ")",
                                        (-1),
                                        (b = new Boolean(),  b.lastIndexOf('o', i )) );
    }

    var origNumberLastIndexOf = Number.prototype.lastIndexOf;
	Number.prototype.lastIndexOf=String.prototype.lastIndexOf;
    for ( var i = 0; i < 9; i++ ) {
        array[item++] = new TestCase(   SECTION,
                                        "var n = new Number(Infinity); n.lastIndexOf = String.prototype.lastIndexOf; n.lastIndexOf( 'i', " + i + " )",
                                        ( i < 3 ? -1 : ( i < 5 ? 3 : 5 ) ),
                                        (n = new Number(Infinity), n.lastIndexOf( 'i', i ) ) );
    }
    var a = new Array( "abc","def","ghi","jkl","mno","pqr","stu","vwx","yz" );
    a.lastIndexOf = String.prototype.lastIndexOf;
    
    for ( var i = 0; i < (a.toString()).length; i++ ) {
        array[item++] = new TestCase( SECTION,
                                      "var a = new Array( 'abc','def','ghi','jkl','mno','pqr','stu','vwx','yz' ); a.lastIndexOf = String.prototype.lastIndexOf; a.lastIndexOf( ',mno,p', "+i+" )",
                                      ( i < 15 ? -1 : 15 ),
                                      (a.lastIndexOf( ',mno,p', i ) ) );
    }


    var origMathLastIndexOf = Math.lastIndexOf;
    for ( var i = 0; i < 15; i ++ ) {
        array[item++] = new TestCase(   SECTION,
                                        "var m = Math; m.lastIndexOf = String.prototype.lastIndexOf; m.lastIndexOf('t', "+ i + ")",
                                        ( i < 9 ? -1 : 9 ),
                                        (m = Math, m.lastIndexOf = String.prototype.lastIndexOf, m.lastIndexOf('t', i)) );
    }

    
    
/*
    for ( var i = 0; i < 15; i++ ) {
        array[item++] = new TestCase(   SECTION,
                                        "var d = new Date(); d.lastIndexOf = String.prototype.lastIndexOf; d.lastIndexOf( '0' )",
                                        (d = new Date(), d.lastIndexOf = String.prototype.lastIndexOf, d.lastIndexOf( '0' ))
                                    )
    }

*/

    //restore
	Boolean.prototype.lastIndexOf = origBooleanLastIndexOf;
	Boolean.prototype.toString = origBooleanToString;
	Number.prototype.lastIndexOf = origNumberLastIndexOf;

    return array;
}

function LastIndexOf( string, search, position ) {
    string = String( string );
    search = String( search );

    position = Number( position )

    if ( isNaN( position ) ) {
        position = Infinity;
    } else {
        position = ToInteger( position );
    }

    result5= string.length;
    result6 = Math.min(Math.max(position, 0), result5);
    result7 = search.length;

    if (result7 == 0) {
        return Math.min(position, result5);
    }

    result8 = -1;

    for ( k = 0; k <= result6; k++ ) {
        if ( k+ result7 > result5 ) {
            break;
        }
        for ( j = 0; j < result7; j++ ) {
            if ( string.charAt(k+j) != search.charAt(j) ){
                break;
            }   else  {
                if ( j == result7 -1 ) {
                    result8 = k;
                }
            }
        }
    }

    return result8;
}
function ToInteger( n ) {
    n = Number( n );
    if ( isNaN(n) ) {
        return 0;
    }
    if ( Math.abs(n) == 0 || Math.abs(n) == Infinity ) {
        return n;
    }

    var sign = ( n < 0 ) ? -1 : 1;

    return ( sign * Math.floor(Math.abs(n)) );
}
