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

    var SECTION = "11.1.4";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Array Initialiser";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    Array_One = []



    array[item++] = new TestCase(   SECTION,
                                    "typeof Array_One",
                                    "object",
                                    typeof Array_One );

    array[item++] = new TestCase(   SECTION,
                                    "Array_One=[]; Array_One.getClass = Object.prototype.toString; Array_One.getClass()",
                                    "[object Array]",
                                    (Array_One.getClass = Object.prototype.toString, Array_One.getClass() ) );

  array[item++] = new TestCase(   SECTION,
                                    "Array_One = []; Array_One.toString == Array.prototype.toString",
                                    true,
                                    (Array_One.toString == Array.prototype.toString ) );

   array[item++] = new TestCase(   SECTION,
                                    "Array_One.length",
                                    0,
                                    Array_One.length );

  Array_Two = [1,2,3]


  array[item++] = new TestCase(   SECTION,
                                    "Array_Two",
                                    "1,2,3",
                                    Array_Two+"" );



  array[item++] = new TestCase(   SECTION,
                                    "typeof Array_Two",
                                    "object",
                                    typeof Array_Two);

  array[item++] = new TestCase(   SECTION,
                                    "Array_Two=[1,2,3]; arr.getClass = Object.prototype.toString; arr.getClass()",
                                    "[object Array]",
                                    (Array_Two.getClass = Object.prototype.toString, Array_Two.getClass() ) );

  array[item++] = new TestCase(   SECTION,
                                    "Array_Two.toString == Array.prototype.toString",
                                    true,
                                    (Array_Two.toString == Array.prototype.toString ) );

  array[item++] = new TestCase(   SECTION,
                                    "Array_Two.length",
                                    3,
                                    Array_Two.length );

   Array_Three = [12345]

  array[item++] = new TestCase(   SECTION,
                                    "typeof Array_Three",
                                    "object",
                                    typeof Array_Three );

  array[item++] = new TestCase(   SECTION,
                                    "Array_Three=[12345]; Array_Three.getClass = Object.prototype.toString; Array_Three.getClass()",
                                    "[object Array]",
                                    (Array_Three.getClass = Object.prototype.toString, Array_Three.getClass() ) );

  Array_Four = [1,2,3,4,5]

  array[item++] = new TestCase(   SECTION,
                                    "Array_Four.toString == Array.prototype.toString",
                                    true,
                                    (Array_Four.toString == Array.prototype.toString ) );

   Array_Five = [,2,3,4,5]

    array[item++] = new TestCase(   SECTION,
                                    "Array_Five.length",
                                    5,
                                    Array_Five.length );

   array[item++] = new TestCase(   SECTION,
                                    "Array_Five[1]",
                                    2,
                                    Array_Five[1] );

   Array_Six = [1,2,3,4,5,6,7,8,9,10,11,12,13,,15,16,17,18,19,20,21,22,23,24,25]

   array[item++] = new TestCase(   SECTION,
                                    "Array_Six.length",
                                    25,
                                    Array_Six.length );

   array[item++] = new TestCase(   SECTION,
                                    "Array_Six[14]",
                                    15,
                                    Array_Six[14] );

   Array_Seven = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,]

   array[item++] = new TestCase(   SECTION,
                                    "Array_Seven.length",
                                    32,
                                    Array_Seven.length );
  Array_Eight=[,,,,,,,,,,,,,,,]

   array[item++] = new TestCase(   SECTION,
                                    "Array_Eight.length",
                                    15,
                                    Array_Eight.length );

   Array_Nine = [,2,3,4,5,6,7,8,9,10,11,,13,14,15,16,17,18,19,,21,22,23,24,25,26,27,28,29,30,31,32,]

   array[item++] = new TestCase(   SECTION,
                                    "Array_Nine.length",
                                    32,
                                    Array_Nine.length );

   array[item++] = new TestCase(   SECTION,
                                    "Array_Nine[1]",
                                    2,
                                    Array_Nine[1] );

  array[item++] = new TestCase(   SECTION,
                                    "Array_Nine[0]",
                                    undefined,
                                    Array_Nine[0] );

   array[item++] = new TestCase(   SECTION,
                                    "Array_Nine[11]",
                                    undefined,
                                    Array_Nine[11] );

   array[item++] = new TestCase(   SECTION,
                                    "Array_Nine[12]",
                                    13,
                                    Array_Nine[12] );

   array[item++] = new TestCase(   SECTION,
                                    "Array_Nine[19]",
                                    undefined,
                                    Array_Nine[19] );

   array[item++] = new TestCase(   SECTION,
                                    "Array_Nine[20]",
                                    21,
                                    Array_Nine[20] );

   array[item++] = new TestCase(   SECTION,
                                    "Array_Nine[32]",
                                    undefined,
                                    Array_Nine[32] );

   var Array_Ten:Array = [[1, 2, 3], [4, 5, 6], [7, 8, 9]];

   array[item++] = new TestCase(   SECTION,
                                    "Multi dimensional array",
                                    6,
                                    Array_Ten[1][2] );

   var obj = new Object();
   obj.prop1 = "Good";
   obj.prop2 = "one";
   for (j in obj){

       var myvar = obj[j];
       if (myvar=="one"){
          break;
       }
       //print(myvar);
   }

   array[item++] = new TestCase(   SECTION,"Using array initializers to dynamically set and retrieve values of an object","one",myvar );






    return ( array );
}

