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
 * The Original Code is JavaScript Engine performance test.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mazie Lobo     mazielobo@netscape.com
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
 * ***** END LICENSE BLOCK ***** 
 *
 *
 * Date:    07 October 2002
 * SUMMARY: Testing |sort()| method of an Array. 
 *          Sorts the elements of an array. If compareFunction is not supplied, 
 *          elements are sorted by converting them to strings and comparing strings 
 *          in lexicographic ("dictionary" or "telephone book," not numerical) order. 
 *          For example, "80" comes before "9" in lexicographic order, but in a numeric 
 *          sort 9 comes before 80. 
 * 
 *          Syntax: sort(compareFunction) 
 *
 *          Parameters: compareFunction - Specifies a function that defines 
 *          the sort order. If omitted, the array is sorted lexicographically 
 *          (in dictionary order) according to the string conversion of each element. 
 */
//-----------------------------------------------------------------------------
var UBOUND=100000;

test();


function test()
{
  var u_bound=UBOUND;
  var status;
  var start;
  var end;
  var i;
  

  // Array containing 8 string elements. Using function that compares strings 
  var arrString = new Array("zebra", "dog", "elephant", "monkey", "cat", "pig", "tiger", "lion")
  status = "arrString.sort(compareStrings)\t";
  start = new Date();
  for(i=0;i<u_bound; i++)
  {
    arrString.sort(compareStrings);
  }
  end = new Date();
  reportTime(status, start, end);

  // Array containing 8 numerical String elements. Using function that compares strings 
  var arrNumStr = new Array("25", "80", "12", "97", "5", "42", "0", "74")
  status = "arrNumStr.sort(compareStrings)\t";
  start = new Date();
  for(i=0;i<u_bound; i++)
  {
    arrNumStr.sort(compareStrings);
  }
  end = new Date();
  reportTime(status, start, end);

  // Array containing 8 numerical elements. Using function that compares strings 
  var arrNumber = new Array(25, 80, 12, 97, 5, 42, 0, 74)
  status = "arrNumber.sort(compareStrings)\t";
  start = new Date();
  for(i=0;i<u_bound; i++)
  {
    arrNumber.sort(compareStrings);
  }
  end = new Date();
  reportTime(status, start, end);

  // Array containing 8 numerical  elements. Using function that compares numbers 
  arrNumber = new Array(25, 80, 12, 97, 5, 42, 0, 74)
  status = "arrNumber.sort(compareNumbers)";
  start = new Date();
  for(i=0;i<u_bound; i++)
  {
    arrNumber.sort(compareNumbers);
  }
  end = new Date();
  reportTime(status, start, end);

  // Array containing 8 mixed numerical and string elements. Using function that compares strings 
  var arrMixed = new Array(25, 80, 12, 97, "5", "42", "0", "74")
  status = "arrMixed.sort(compareStrings)\t";
  start = new Date();
  for(i=0;i<u_bound; i++)
  {
    arrMixed.sort(compareStrings);
  }
  end = new Date();
  reportTime(status, start, end);

  // Array containing 8 mixed numerical and string elements. Using function that compares numbers 
  arrMixed = new Array(25, 80, 12, 97, "5", "42", "0", "74")
  status = "arrMixed.sort(compareNumbers)";
  start = new Date();
  for(i=0;i<u_bound; i++)
  {
    arrMixed.sort(compareNumbers);
  }
  end = new Date();
  reportTime(status, start, end);
}

  
//Compares strings
function compareStrings(a, b) 
{
   if (a < b )
      return -1
   if (a > b)
      return 1;
   // a must be equal to b
   return 0;
} 

//Compares numbers instead of string
function compareNumbers(a, b) 
{
   return a - b
}
