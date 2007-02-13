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
    var SECTION = "15.5.4.6-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.protoype.indexOf";

    var TEST_STRING = new String( " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~" );

    writeHeaderToLog( SECTION + " "+ TITLE);
    writeLineToLog( "TEST_STRING = new String(\" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\")" );

    var testcases = getTestCases();
    test();


function getTestCases() {
    var array = new Array();
    var j = 0;

    for ( k = 0, i = 0x0020; i < 0x007e; i++, j++, k++ ) {
        array[j] = new TestCase( SECTION,
                                 "String.indexOf(" +String.fromCharCode(i)+ ", 0)",
                                 k,
                                 TEST_STRING.indexOf( String.fromCharCode(i), 0 ) );
    }

    for ( k = 0, i = 0x0020; i < 0x007e; i++, j++, k++ ) {
        array[j] = new TestCase( SECTION,
                                 "String.indexOf("+String.fromCharCode(i)+ ", "+ k +")",
                                 k,
                                 TEST_STRING.indexOf( String.fromCharCode(i), k ) );
    }

    for ( k = 0, i = 0x0020; i < 0x007e; i++, j++, k++ ) {
        array[j] = new TestCase( SECTION,
                                 "String.indexOf("+String.fromCharCode(i)+ ", "+k+1+")",
                                 -1,
                                 TEST_STRING.indexOf( String.fromCharCode(i), k+1 ) );
    }

    for ( k = 0, i = 0x0020; i < 0x007d; i++, j++, k++ ) {
        array[j] = new TestCase( SECTION,
                                 "String.indexOf("+(String.fromCharCode(i) +
                                                    String.fromCharCode(i+1)+
                                                    String.fromCharCode(i+2)) +", "+0+")",
                                 k,
                                 TEST_STRING.indexOf( (String.fromCharCode(i)+
                                                       String.fromCharCode(i+1)+
                                                       String.fromCharCode(i+2)),
                                                      0 ) );
    }

    for ( k = 0, i = 0x0020; i < 0x007d; i++, j++, k++ ) {
        array[j] = new TestCase( SECTION,
                                 "String.indexOf("+(String.fromCharCode(i) +
                                                    String.fromCharCode(i+1)+
                                                    String.fromCharCode(i+2)) +", "+ k +")",
                                 k,
                                 TEST_STRING.indexOf( (String.fromCharCode(i)+
                                                       String.fromCharCode(i+1)+
                                                       String.fromCharCode(i+2)),
                                                       k ) );
    }
    for ( k = 0, i = 0x0020; i < 0x007d; i++, j++, k++ ) {
        array[j] = new TestCase( SECTION,
                                 "String.indexOf("+(String.fromCharCode(i) +
                                                    String.fromCharCode(i+1)+
                                                    String.fromCharCode(i+2)) +", "+ k+1 +")",
                                 -1,
                                 TEST_STRING.indexOf( (String.fromCharCode(i)+
                                                       String.fromCharCode(i+1)+
                                                       String.fromCharCode(i+2)),
                                                       k+1 ) );
    }

    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING + ", 0 )", 0, TEST_STRING.indexOf( TEST_STRING, 0 ) );
    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING + ", 1 )", -1, TEST_STRING.indexOf( TEST_STRING, 1 ));
   
    var TEST_STRING2 = " ";
    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING2 + ", 0 )", 0, TEST_STRING.indexOf( TEST_STRING2, 0 ));
    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING2 + ", 1 )", -1, TEST_STRING.indexOf( TEST_STRING2, 1 ));
    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING2 + ", 1 )", 0, TEST_STRING.indexOf( TEST_STRING2));
   
    var TEST_STRING3:String = "abc def";
   
    for (i=0;i<7;i++){
        array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING3 + ", i )", i, TEST_STRING3.indexOf( TEST_STRING3.charAt(i)));
    }
   
    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING3 + ", 8 )",0, TEST_STRING3.indexOf( TEST_STRING3.charAt(8)));
    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING3+ " )",-1, TEST_STRING3.indexOf());
    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING3+ " )",3, TEST_STRING3.indexOf(" "));
    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING3+ " )",-1, TEST_STRING3.indexOf(null));
    array[j++] = new TestCase( SECTION,  "String.indexOf(" +TEST_STRING3+ " )",-1, TEST_STRING3.indexOf(null,0));


    return array;
}
