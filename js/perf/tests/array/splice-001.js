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
 * Date:    04 October 2002
 * SUMMARY: Testing |splice()| method of an Array. 
 *          Changes the content of an array, adding new elements while removing old elements.
 *
 *          Syntax: splice(index, howMany, [element1][, ..., elementN]) 
 *
 *          Parameters: index - index at which to start changing the array.  
 *          howMany - An integer indicating the number of old array elements to remove.
 *          If howMany is 0, no elements are removed. In this case, you should specify at least one new element.  
 *          element1, ...,elementN - The elements to add to the array. If you don't specify any elements, 
 *          splice simply removes elements from the array.  
 */
//-----------------------------------------------------------------------------
var UBOUND=10000;

test();


function test()
{
  var u_bound=UBOUND;
  var status;
  var start;
  var end;
  var i;
  var j;
  var elt0, elt1, elt2, elt3, elt4, elt5, elt6, elt7, elt8, elt9;
  var elt10, elt11, elt12;
  var arr0, arr1;


  // Array containing 10 elements 
  arr0 = new Array(elt0, elt1, elt2, elt3, elt4, elt5, elt6, elt7, elt8, elt9);

  // Does not remove an element, but adds an element.
  status = "arr0.splice(2,0,elt10)\t\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    arr0.splice(2,0,elt10);
  }
  end = new Date();
  reportTime(status, start, end);

  //  Removes an element, but does not add an element
  arr0 = new Array(elt0, elt1, elt2, elt3, elt4, elt5, elt6, elt7, elt8, elt9);
  arr1=arr0
  status = "arr1.splice(3,1) \t\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    for(j=0;j<9; j++)
	{
	  arr1.splice(3,1);
    }
	arr1=arr0;
  }
  end = new Date();
  reportTime(status, start, end);

  //  Removes an element, and replaces an element
  arr0 = new Array(elt0, elt1, elt2, elt3, elt4, elt5, elt6, elt7, elt8, elt9);
  status = "arr0.splice(2,1,'abc') \t\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
      arr0.splice(2,1,elt10);
  }
  end = new Date();
  reportTime(status, start, end);

  //  Removes an element, and replaces it with 3 elements
  arr0 = new Array(elt0, elt1, elt2, elt3, elt4, elt5, elt6, elt7, elt8, elt9);
  status = 'arr0.splice(0,2,"abc","def","ghi")';
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    arr0.splice(0,2,elt10,elt11,elt12);
  }
  end = new Date();
  reportTime(status, start, end);
}
