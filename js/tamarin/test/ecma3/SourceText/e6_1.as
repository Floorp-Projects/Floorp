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

    var SECTION = "6-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Source Text";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    array[item++] = new TestCase( SECTION,
                                    "// the following character should not be interpreted as a line terminator in a comment: \u000A",
                                    'PASSED',
                                    "PASSED" );

    // \u000A array[item++].actual = "FAILED!";

    array[item++] = new TestCase( SECTION,
                                    "// the following character should not be interpreted as a line terminator in a comment: \\n 'FAILED'",
                                    'PASSED',
                                   'PASSED' );

    // the following character should not be interpreted as a line terminator: \\n array[item++].actual = "FAILED"

    array[item++] = new TestCase( SECTION,
                                    "// the following character should not be interpreted as a line terminator in a comment: \\u000A 'FAILED'",
                                    'PASSED',
                                    'PASSED' )

    // the following character should not be interpreted as a line terminator:   \u000A array[item++].actual = "FAILED"

    array[item++] = new TestCase( SECTION,
                                    "// the following character should not be interpreted as a line terminator in a comment: \n 'PASSED'",
                                    'PASSED',
                                    'PASSED' )
    // the following character should not be interpreted as a line terminator: \n array[item++].actual = 'FAILED'

    array[item++] = new TestCase(   SECTION,
                                    "// the following character should not be interpreted as a line terminator in a comment: u000D",
                                    'PASSED',
                                    'PASSED' )

    // the following character should not be interpreted as a line terminator:   \u000D array[item++].actual = "FAILED"

    array[item++] = new TestCase(   SECTION,
                                    "// the following character should not be interpreted as a line terminator in a comment: u2028",
                                    'PASSED',
                                    'PASSED' )

    // the following character should not be interpreted as a line terminator:   \u2028 array[item++].actual = "FAILED"

    array[item++] = new TestCase(   SECTION,
                                    "// the following character should not be interpreted as a line terminator in a comment: u2029",
                                    'PASSED',
                                    'PASSED' )

    // the following character should not be interpreted as a line terminator:   \u2029 array[item++].actual = "FAILED"

    array[item++] = new TestCase(   SECTION,
                                    "the following character should  be interpreted as  backspace in a string: \u0008",'abc\bdef',
                                    'abc\u0008def' )
    array[item++] = new TestCase(   SECTION,
                                    "the following character should  be interpreted as  whitespace(Tab) in a string: \u0009",'abc\tdef',
                                    'abc\u0009def' )
    array[item++] = new TestCase(   SECTION,
                                    "the following character should  be interpreted as  vertical tab in a string: \u000B",'abc\vdef',
                                    'abc\u000Bdef' )

    array[item++] = new TestCase(   SECTION,
                                    "the following character should  be interpreted as  form feed in a string: \u000C",'abc\fdef','abc\u000Cdef' )
  

    array[item++] = new TestCase(   SECTION,
                                    "the following character should  be interpreted as  double quotes in a string and should not terminate a string but should just add a character to the string: \u0022",'abc\"def',
                                    'abc\u0022def' )

    array[item++] = new TestCase(   SECTION,
                                    "the following character should  be interpreted as  single quote in a string and should not terminate a string but should just add a character to the string : \u0027",'abc\'def',
                                    'abc\u0027def' )

    array[item++] = new TestCase(   SECTION,
                                    "the following character should  be interpreted as  single blackslash in a string : \u005C",'abc\\def','abc\u005Cdef' )
  
    array[item++] = new TestCase(   SECTION,
                                    "the following character should  be interpreted as  space in a string : \u0020",'abc def','abc\u0020def' )


    return array;
}
