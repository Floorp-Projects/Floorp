/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com  
* Date: 12 MAR 2001
*
*
* SUMMARY: Testing Array.prototype.toLocaleString()
* See http://bugzilla.mozilla.org/show_bug.cgi?id=56883
* See http://bugzilla.mozilla.org/show_bug.cgi?id=58031
*  
*/
//-------------------------------------------------------------------------------------------------
var bug = 56883;
var summary = 'Testing Array.prototype.toLocaleString() -';
var actual = '';
var expect = '';


/* According to ECMA3 15.4.4.3, a.toLocaleString() means toLocaleString()
 * should be applied to each element of the array, and the results should be
 * concatenated with an implementation-specific delimiter. For example: 
 *
 *              a[0].toLocaleString()  +    ','  +   a[1].toLocaleString()   +  etc.
 *
 * Below, toLocaleString() is found as a property of each element of the array,
 * and is the function to be invoked. Since it increments the global count 
 * variable by one each time, the end-value of count should be a.length.
 */
var count =0;
var x = {toLocaleString: function() {count++}};
var a = [x,x,x];
a.toLocaleString();
actual=count;
expect=a.length; // see explanation above


//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function test() 
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);
  
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
