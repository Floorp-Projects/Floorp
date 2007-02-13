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
    var SECTION = "dowhile-007";
    var VERSION = "ECMA_2";
    var TITLE   = "do...while";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    DoWhile( new DoWhileObject( false, false, false, false ));
    DoWhile( new DoWhileObject( true, false, false, false ));
    DoWhile( new DoWhileObject( true, true, false, false ));
    DoWhile( new DoWhileObject( true, true, true, false ));
    DoWhile( new DoWhileObject( true, true, true, true ));
    DoWhile( new DoWhileObject( false, false, false, true ));
    DoWhile( new DoWhileObject( false, false, true, true ));
    DoWhile( new DoWhileObject( false, true, true, true ));
    DoWhile( new DoWhileObject( false, false, true, false ));


    function DoWhileObject( out1, out2, out3, in1 ) {
        this.breakOutOne = out1;
        this.breakOutTwo = out2;
        this.breakOutThree = out3;
        this.breakIn = in1;
    }
    function DoWhile( object ) {
        result1 = false;
        result2 = false;
        result3 = false;
        result4 = false;
    
        outie:
            do {
                if ( object.breakOutOne ) {
                    break outie;
                }
                result1 = true;
    
                innie:
                    do {
                        if ( object.breakOutTwo ) {
                            break outie;
                        }
                        result2 = true;
    
                        if ( object.breakIn ) {
                            break innie;
                        }
                        result3 = true;
    
                    } while ( false );
                        if ( object.breakOutThree ) {
                            break outie;
                        }
                        result4 = true;
            } while ( false );
    
            array[item++] = new TestCase(
                SECTION,
                "break one: ",
                (object.breakOutOne) ? false : true,
                result1 );
    
            array[item++] = new TestCase(
                SECTION,
                "break two: ",
                (object.breakOutOne||object.breakOutTwo) ? false : true,
                result2 );
    
            array[item++] = new TestCase(
                SECTION,
                "break three: ",
                (object.breakOutOne||object.breakOutTwo||object.breakIn) ? false : true,
                result3 );
    
            array[item++] = new TestCase(
                SECTION,
                "break four: ",
                (object.breakOutOne||object.breakOutTwo||object.breakOutThree) ? false: true,
                result4 );
    }
    return array;
}
