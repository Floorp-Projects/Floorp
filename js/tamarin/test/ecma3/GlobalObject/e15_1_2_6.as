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
    var SECTION = "15.1.2.6";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "isNaN( x )";

    var BUGNUMBER = "77391";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();

    test();


function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION, "isNaN.length",      1,                  isNaN.length );

    var MYPROPS='';
    for ( var p in isNaN ) {
       MYPROPS+= p;
    }

    array[item++] = new TestCase( SECTION, "var MYPROPS='', for ( var p in isNaN ) { MYPROPS+= p }, MYPROPS", "", MYPROPS );

    var thisError:String = "no error";
    try
    {
        isNaN.length=null;
    }
    catch (e:ReferenceError)
    {
        thisError = e.toString();
    }
    finally
    {
        array[item++] = new TestCase(SECTION, "isNaN.length = null", "ReferenceError: Error #1074", referenceError(thisError));
    }
    array[item++] = new TestCase( SECTION, "delete isNaN.length",               false,  delete isNaN.length );
    //array[item++] = new TestCase( SECTION, "delete isNaN.length; isNaN.length", 1,      eval("delete isNaN.length; isNaN.length") );
    delete isNaN.length;
    array[item++] = new TestCase( SECTION, "delete isNaN.length; isNaN.length", 1, isNaN.length);

//    array[item++] = new TestCase( SECTION, "isNaN.__proto__",   Function.prototype, isNaN.__proto__ );

    array[item++] = new TestCase( SECTION, "isNaN()",           true,               isNaN() );
    array[item++] = new TestCase( SECTION, "isNaN( null )",     false,              isNaN(null) );
    array[item++] = new TestCase( SECTION, "isNaN( void 0 )",   true,               isNaN(void 0) );
    array[item++] = new TestCase( SECTION, "isNaN( true )",     false,              isNaN(true) );
    array[item++] = new TestCase( SECTION, "isNaN( false)",     false,              isNaN(false) );
    array[item++] = new TestCase( SECTION, "isNaN( ' ' )",      false,              isNaN( " " ) );

    array[item++] = new TestCase( SECTION, "isNaN( 0 )",        false,              isNaN(0) );
    array[item++] = new TestCase( SECTION, "isNaN( 1 )",        false,              isNaN(1) );
    array[item++] = new TestCase( SECTION, "isNaN( 2 )",        false,              isNaN(2) );
    array[item++] = new TestCase( SECTION, "isNaN( 3 )",        false,              isNaN(3) );
    array[item++] = new TestCase( SECTION, "isNaN( 4 )",        false,              isNaN(4) );
    array[item++] = new TestCase( SECTION, "isNaN( 5 )",        false,              isNaN(5) );
    array[item++] = new TestCase( SECTION, "isNaN( 6 )",        false,              isNaN(6) );
    array[item++] = new TestCase( SECTION, "isNaN( 7 )",        false,              isNaN(7) );
    array[item++] = new TestCase( SECTION, "isNaN( 8 )",        false,              isNaN(8) );
    array[item++] = new TestCase( SECTION, "isNaN( 9 )",        false,              isNaN(9) );
    array[item++] = new TestCase( SECTION, "isNaN( -1 )",       false,              isNaN(-1) );

    array[item++] = new TestCase( SECTION, "isNaN( '-1' )",     false,              isNaN(-1) );

    array[item++] = new TestCase( SECTION, "isNaN( 1.23 )",     false,              isNaN(1.23) );

    array[item++] = new TestCase( SECTION, "isNaN('1.23')",     false,              isNaN(1.23) );

    array[item++] = new TestCase( SECTION, "isNaN( -1.23 )",    false,              isNaN(-1.23) );

    array[item++] = new TestCase( SECTION, "isNaN( '-1.23' )",  false,              isNaN(-1.23) );

    array[item++] = new TestCase( SECTION, "isNaN( '0' )",        false,              isNaN('0') );
    array[item++] = new TestCase( SECTION, "isNaN( '1' )",        false,              isNaN('1') );
    array[item++] = new TestCase( SECTION, "isNaN( '2' )",        false,              isNaN('2') );
    array[item++] = new TestCase( SECTION, "isNaN( '3' )",        false,              isNaN('3') );
    array[item++] = new TestCase( SECTION, "isNaN( '4' )",        false,              isNaN('4') );
    array[item++] = new TestCase( SECTION, "isNaN( '5' )",        false,              isNaN('5') );
    array[item++] = new TestCase( SECTION, "isNaN( '6' )",        false,              isNaN('6') );
    array[item++] = new TestCase( SECTION, "isNaN( '7' )",        false,              isNaN('7') );
    array[item++] = new TestCase( SECTION, "isNaN( '8' )",        false,              isNaN('8') );
    array[item++] = new TestCase( SECTION, "isNaN( '9' )",        false,              isNaN('9') );


    array[item++] = new TestCase( SECTION, "isNaN( 0x0a )",    false,              isNaN( 0x0a ) );
    array[item++] = new TestCase( SECTION, "isNaN( 0xaa )",    false,              isNaN( 0xaa ) );
    array[item++] = new TestCase( SECTION, "isNaN( 0x0A )",    false,              isNaN( 0x0A ) );
    array[item++] = new TestCase( SECTION, "isNaN( 0xAA )",    false,              isNaN( 0xAA ) );

    array[item++] = new TestCase( SECTION, "isNaN( '0x0a' )",  false,              isNaN( "0x0a" ) );
    array[item++] = new TestCase( SECTION, "isNaN( '0xaa' )",  false,              isNaN( "0xaa" ) );
    array[item++] = new TestCase( SECTION, "isNaN( '0x0A' )",  false,              isNaN( "0x0A" ) );
    array[item++] = new TestCase( SECTION, "isNaN( '0xAA' )",  false,              isNaN( "0xAA" ) );

    array[item++] = new TestCase( SECTION, "isNaN( 077 )",     false,              isNaN( 077 ) );
    array[item++] = new TestCase( SECTION, "isNaN( '077' )",   false,              isNaN( "077" ) );


    array[item++] = new TestCase( SECTION, "isNaN( Number.NaN )",   true,              isNaN(Number.NaN) );
    array[item++] = new TestCase( SECTION, "isNaN( Number.POSITIVE_INFINITY )", false,  isNaN(Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION, "isNaN( Number.NEGATIVE_INFINITY )", false,  isNaN(Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION, "isNaN( Number.MAX_VALUE )",         false,  isNaN(Number.MAX_VALUE) );
    array[item++] = new TestCase( SECTION, "isNaN( Number.MIN_VALUE )",         false,  isNaN(Number.MIN_VALUE) );

    array[item++] = new TestCase( SECTION, "isNaN( NaN )",               true,      isNaN(NaN) );
    array[item++] = new TestCase( SECTION, "isNaN( Infinity )",          false,     isNaN(Infinity) );

    array[item++] = new TestCase( SECTION, "isNaN( 'Infinity' )",               false,  isNaN("Infinity") );
    array[item++] = new TestCase( SECTION, "isNaN( '-Infinity' )",              false,  isNaN("-Infinity") );

    array[item++] = new TestCase( SECTION, "isNaN( 'string' )",              true,  isNaN("string") );

    array[item++] = new TestCase( SECTION, "isNaN({} )",true,  isNaN({}));

    array[item++] = new TestCase( SECTION, "isNaN(undefined)",true,  isNaN(undefined));


    var arr= new Array();
    array[item++] = new TestCase( SECTION, "isNaN(arr)",false,  isNaN(arr));

    var obj = new Object();
    array[item++] = new TestCase( SECTION, "isNaN(obj)",true,  isNaN(obj));

    var mydate = new Date(0);
    array[item++] = new TestCase( SECTION, "isNaN(mydate)",false,  isNaN(mydate));
    return ( array );
}
