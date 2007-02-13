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
    var SECTION = "15.1.2.4";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "escape(string)";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();

    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION, "escape.length",         1,          escape.length );

    var thisError:String = "no error";
    try
    {
        escape.length = null;
    }
    catch (e:ReferenceError)
    {
        thisError = e.toString();
    }
    finally
    {
        array[item++] = new TestCase(SECTION, "escape.length = null", "ReferenceError: Error #1074", referenceError(thisError));
    }

    array[item++] = new TestCase( SECTION, "delete escape.length",                  false,  delete escape.length );
    delete escape.length;
    array[item++] = new TestCase( SECTION, "delete escape.length; escape.length",   1, escape.length);

    var MYPROPS='';
    for ( var p in escape ) {
        MYPROPS+= p;
    }

    array[item++] = new TestCase( SECTION, "var MYPROPS='', for ( var p in escape ) { MYPROPS+= p}, MYPROPS",    "",    MYPROPS );

    array[item++] = new TestCase( SECTION, "escape()",              "undefined",    escape() );
    array[item++] = new TestCase( SECTION, "escape('')",            "",             escape('') );
    array[item++] = new TestCase( SECTION, "escape( null )",        "null",         escape(null) );
    array[item++] = new TestCase( SECTION, "escape( void 0 )",      "null",    escape(void 0) );
    array[item++] = new TestCase( SECTION, "escape( true )",        "true",         escape( true ) );
    array[item++] = new TestCase( SECTION, "escape( false )",       "false",        escape( false ) );

    array[item++] = new TestCase( SECTION, "escape( new Boolean(true) )",   "true", escape(new Boolean(true)) );
    array[item++] = new TestCase( SECTION, "escape( new Boolean(false) )",  "false",    escape(new Boolean(false)) );

    array[item++] = new TestCase( SECTION, "escape( Number.NaN  )",                 "NaN",      escape(Number.NaN) );
    array[item++] = new TestCase( SECTION, "escape( -0 )",                          "0",        escape( -0 ) );
    array[item++] = new TestCase( SECTION, "escape( 'Infinity' )",                  "Infinity", escape( "Infinity" ) );
    array[item++] = new TestCase( SECTION, "escape( Number.POSITIVE_INFINITY )",    "Infinity", escape( Number.POSITIVE_INFINITY ) );
    array[item++] = new TestCase( SECTION, "escape( Number.NEGATIVE_INFINITY )",    "-Infinity", escape( Number.NEGATIVE_INFINITY ) );

    var ASCII_TEST_STRING = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@*_+-./";

    array[item++] = new TestCase( SECTION, "escape( " +ASCII_TEST_STRING+" )",    ASCII_TEST_STRING,  escape( ASCII_TEST_STRING ) );

    // ASCII value less than

    for ( var CHARCODE = 0; CHARCODE < 32; CHARCODE++ ) {
        array[item++] = new TestCase( SECTION,
                            "escape(String.fromCharCode("+CHARCODE+"))",
                            "%"+ToHexString(CHARCODE),
                            escape(String.fromCharCode(CHARCODE))  );
    }
    for ( var CHARCODE = 128; CHARCODE < 256; CHARCODE++ ) {
        array[item++] = new TestCase( SECTION,
                            "escape(String.fromCharCode("+CHARCODE+"))",
                            "%"+ToHexString(CHARCODE),
                            escape(String.fromCharCode(CHARCODE))  );
    }

    for ( var CHARCODE = 256; CHARCODE < 1024; CHARCODE++ ) {
        array[item++] = new TestCase( SECTION,
                            "escape(String.fromCharCode("+CHARCODE+"))",
                            "%u"+ ToUnicodeString(CHARCODE),
                            escape(String.fromCharCode(CHARCODE))  );
    }
    for ( var CHARCODE = 65500; CHARCODE < 65536; CHARCODE++ ) {
        array[item++] = new TestCase( SECTION,
                            "escape(String.fromCharCode("+CHARCODE+"))",
                            "%u"+ ToUnicodeString(CHARCODE),
                            escape(String.fromCharCode(CHARCODE))  );
    }

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
