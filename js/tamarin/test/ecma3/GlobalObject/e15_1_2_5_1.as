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

    var SECTION = "15.1.2.5-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "unescape(string)";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();

    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION, "unescape.length",       1,               unescape.length );
    //array[item++] = new TestCase( SECTION, "unescape.length = null; unescape.length",   1,      eval("unescape.length=null; unescape.length") );

    var thisError:String
    try
    {
        unescape.length=null;
    }
    catch(e:ReferenceError)
    {
        thisError = e.toString();
    }
    finally
    {
        array[item++] = new TestCase(SECTION, "unescape.length = null", "ReferenceError: Error #1074", referenceError(thisError));
    }
    array[item++] = new TestCase( SECTION, "delete unescape.length",                    false,  delete unescape.length );
    //array[item++] = new TestCase( SECTION, "delete unescape.length; unescape.length",   1,      eval("delete unescape.length; unescape.length") );
    delete unescape.length;
    array[item++] = new TestCase( SECTION, "delete unescape.length; unescape.length",   1,      unescape.length);

    var MYPROPS='';
    for ( var p in unescape ) {
        MYPROPS+= p;
    }

    array[item++] = new TestCase( SECTION, "var MYPROPS='', for ( var p in unescape ) { MYPROPS+= p }, MYPROPS",    "", MYPROPS );

    array[item++] = new TestCase( SECTION, "unescape()",              "undefined",    unescape() );
    array[item++] = new TestCase( SECTION, "unescape('')",            "",             unescape('') );
    array[item++] = new TestCase( SECTION, "unescape( null )",        "null",         unescape(null) );
    array[item++] = new TestCase( SECTION, "unescape( void 0 )",      "null",    unescape(void 0) );
    array[item++] = new TestCase( SECTION, "unescape( true )",        "true",         unescape( true ) );
    array[item++] = new TestCase( SECTION, "unescape( false )",       "false",        unescape( false ) );

    array[item++] = new TestCase( SECTION, "unescape( new Boolean(true) )",   "true", unescape(new Boolean(true)) );
    array[item++] = new TestCase( SECTION, "unescape( new Boolean(false) )",  "false",    unescape(new Boolean(false)) );

    array[item++] = new TestCase( SECTION, "unescape( Number.NaN  )",                 "NaN",      unescape(Number.NaN) );
    array[item++] = new TestCase( SECTION, "unescape( -0 )",                          "0",        unescape( -0 ) );
    array[item++] = new TestCase( SECTION, "unescape( 'Infinity' )",                  "Infinity", unescape( "Infinity" ) );
    array[item++] = new TestCase( SECTION, "unescape( Number.POSITIVE_INFINITY )",    "Infinity", unescape( Number.POSITIVE_INFINITY ) );
    array[item++] = new TestCase( SECTION, "unescape( Number.NEGATIVE_INFINITY )",    "-Infinity", unescape( Number.NEGATIVE_INFINITY ) );

    var ASCII_TEST_STRING = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@*_+-./";

    array[item++] = new TestCase( SECTION, "unescape( " +ASCII_TEST_STRING+" )",    ASCII_TEST_STRING,  unescape( ASCII_TEST_STRING ) );

    // escaped chars with ascii values less than 256

    for ( var CHARCODE = 0; CHARCODE < 256; CHARCODE++ ) {
        array[item++] = new TestCase( SECTION,
                            "unescape( %"+ ToHexString(CHARCODE)+" )",
                            String.fromCharCode(CHARCODE),
                            unescape( "%" + ToHexString(CHARCODE) )  );
    }

    // unicode chars represented by two hex digits
    for ( var CHARCODE = 0; CHARCODE < 256; CHARCODE++ ) {
        array[item++] = new TestCase( SECTION,
                            "unescape( %u"+ ToHexString(CHARCODE)+" )",
                            "%u"+ToHexString(CHARCODE),
                            unescape( "%u" + ToHexString(CHARCODE) )  );
    }
/*
    for ( var CHARCODE = 0; CHARCODE < 256; CHARCODE++ ) {
        array[item++] = new TestCase( SECTION,
                            "unescape( %u"+ ToUnicodeString(CHARCODE)+" )",
                            String.fromCharCode(CHARCODE),
                            unescape( "%u" + ToUnicodeString(CHARCODE) )  );
    }
    for ( var CHARCODE = 256; CHARCODE < 65536; CHARCODE+= 333 ) {
        array[item++] = new TestCase( SECTION,
                            "unescape( %u"+ ToUnicodeString(CHARCODE)+" )",
                            String.fromCharCode(CHARCODE),
                            unescape( "%u" + ToUnicodeString(CHARCODE) )  );
    }
*/
    return ( array );
}

function ToUnicodeString( n ) {
    var string = ToHexString(n);

    for ( var PAD = (4 - string.length ); PAD > 0; PAD-- ) {
        string = "0" + string;
    }

    return string;
}
function ToHexString( n ) {
    var hex = new Array();

    for ( var mag = 1; Math.pow(16,mag) <= n ; mag++ ) {
        ;
    }

    for ( index = 0, mag -= 1; mag > 0; index++, mag-- ) {
        hex[index] = Math.floor( n / Math.pow(16,mag) );
        n -= Math.pow(16,mag) * Math.floor( n/Math.pow(16,mag) );
    }

    hex[hex.length] = n % 16;

    var string ="";

    for ( var index = 0 ; index < hex.length ; index++ ) {
        switch ( hex[index] ) {
            case 10:
                string += "A";
                break;
            case 11:
                string += "B";
                break;
            case 12:
                string += "C";
                break;
            case 13:
                string += "D";
                break;
            case 14:
                string += "E";
                break;
            case 15:
                string += "F";
                break;
            default:
                string += hex[index];
        }
    }

    if ( string.length == 1 ) {
        string = "0" + string;
    }
    return string;
}
