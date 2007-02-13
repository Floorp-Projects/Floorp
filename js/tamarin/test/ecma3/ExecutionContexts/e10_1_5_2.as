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
    var SECTION = "10.5.1-2";
    var VERSION = "ECMA_1";
    startTest();

    writeHeaderToLog( SECTION + " Global Ojbect");

    var testcases = getTestCases();

    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[0] = new TestCase( "SECTION", "Eval Code check" );

    var EVAL_STRING = 'if ( Object == null ) { array[0].reason += " Object == null" ; }' +
        'if ( Function == null ) { array[0].reason += " Function == null"; }' +
        'if ( String == null ) { array[0].reason += " String == null"; }'   +
        'if ( Array == null ) { array[0].reason += " Array == null"; }'     +
        'if ( Number == null ) { array[0].reason += " Function == null";}'  +
        'if ( Math == null ) { array[0].reason += " Math == null"; }'       +
        'if ( Boolean == null ) { array[0].reason += " Boolean == null"; }' +
        'if ( Date  == null ) { array[0].reason += " Date == null"; }'      +
        //'if ( eval == null ) { array[0].reason += " eval == null"; }'       +
        'if ( parseInt == null ) { array[0].reason += " parseInt == null"; }' ;

    //eval( EVAL_STRING );
    EVAL_STRING;

/*
    if ( NaN == null ) {
        array[0].reason += " NaN == null";
    }
    if ( Infinity == null ) {
        array[0].reason += " Infinity == null";
    }
*/

    if ( array[0].reason != "" ) {
        array[0].actual = "fail";
    } else {
        array[0].actual = "pass";
    }
    array[0].expect = "pass";

    return ( array );
}
