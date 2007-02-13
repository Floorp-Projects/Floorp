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

    var SECTION = "15.5.4.9";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.localeCompare";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    
    var str1 = new String("h");	
    var str2 = new String ("H");
    array[item++] = new TestCase( SECTION,  "str1.localeCompare(str2)", 32,  str1.localeCompare(str2) );

    var str2 = new String ("hello");
    array[item++] = new TestCase( SECTION,  "str1.localeCompare(str2)", -1,  str1.localeCompare(str2) );

    var str2 = new String ("flash");
    array[item++] = new TestCase( SECTION,  "str1.localeCompare(str2)", 2,  str1.localeCompare(str2) )
    
    var str1 = "h"
    var str2 = "H"
    var thisError = "no error";
    try{
       String.prototype.localeCompare(str2,str1)
    }catch(e:ArgumentError){
       thisError = (e.toString()).substring(0,26);
    }finally{
         array[item++] = new TestCase( SECTION,  "String.prototype.localeCompare(str2,str1)","ArgumentError: Error #1063",thisError)
    }

    /*var str2 = "hello";
    array[item++] = new TestCase( SECTION,  "localeCompare(str1,str2)",-13,  String.prototype.localeCompare(str1,str2) )

    var str2 = "flash"
    array[item++] = new TestCase( SECTION,  "localeCompare(str1,str2)",-11,  String.prototype.localeCompare(str2,str1) )*/
    

    return ( array );
}
