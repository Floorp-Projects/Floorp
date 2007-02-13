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
    var SECTION = "15.5.4.7-3";
    var VERSION = "ECMA_2";
    startTest();
    var TITLE   = "String.protoype.lastIndexOf";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();


function getTestCases() {
    var array = new Array();
    var item = 0;

    var thisError="no error";
    var b = true;
    try{
        b.__proto__.lastIndexOf = String.prototype.lastIndexOf;
        b.lastIndexOf('r', 0 );
    }catch(e:Error){
        thisError=e.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r', 0 )","ReferenceError: Error #1069"
,referenceError(thisError));
    }

   /* array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r', 0 )",
                                    -1,
                                    (b = true, b.__proto__.lastIndexOf = String.prototype.lastIndexOf, b.lastIndexOf('r', 0 ) ) );*/

    thisError="no error";
    var b = true;
    try{
        b.__proto__.lastIndexOf = String.prototype.lastIndexOf;
        b.lastIndexOf('r', 1 );
    }catch(e1:Error){
        thisError=e1.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r', 1 )","ReferenceError: Error #1069"
,referenceError(thisError));
    }

   /* array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r', 1 )",
                                    1,
                                    (b = true, b.__proto__.lastIndexOf = String.prototype.lastIndexOf, b.lastIndexOf('r', 1 ) ) );*/

    thisError="no error";
    var b = true;
    try{
        b.__proto__.lastIndexOf = String.prototype.lastIndexOf;
        b.lastIndexOf('r', 2 );
    }catch(e2:Error){
        thisError=e2.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r', 2 )","ReferenceError: Error #1069"
,referenceError(thisError));
    }

   /* array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r', 2 )",
                                    1,
                                    (b = true, b.__proto__.lastIndexOf = String.prototype.lastIndexOf, b.lastIndexOf('r', 2 ) ) );*/

    thisError="no error";
    var b = true;
    try{
        b.__proto__.lastIndexOf = String.prototype.lastIndexOf;
        b.lastIndexOf('r', 10 );
    }catch(e10:Error){
        thisError=e10.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r', 10 )","ReferenceError: Error #1069"
,referenceError(thisError));
    }

  /*array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r', 10 )",
                                    1,
                                    (b = true, b.__proto__.lastIndexOf = String.prototype.lastIndexOf, b.lastIndexOf('r', 10 ) ) );*/
    thisError="no error";
    var b = true;
    try{
        b.__proto__.lastIndexOf = String.prototype.lastIndexOf;
        b.lastIndexOf('r' );
    }catch(e3:Error){
        thisError=e3.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r')","ReferenceError: Error #1069"
,referenceError(thisError));
    }

    /*array[item++] = new TestCase(   SECTION,
                                    "var b = true; b.__proto__.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('r' )",
                                    1,
                                    (b = true, b.__proto__.lastIndexOf = String.prototype.lastIndexOf, b.lastIndexOf('r' ) ) );*/

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
