/* ***** BEGIN LICENSE BLOCK *****
* Version: NPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Netscape Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is JavaScript Engine testing utilities.
*
* The Initial Developer of the Original Code is Netscape Communications Corp.
* Portions created by the Initial Developer are Copyright (C) 2002
* the Initial Developer. All Rights Reserved.
*
* Contributor(s): zen-parse@gmx.net, pschwartau@netscape.com
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the NPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the NPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK *****
*
*
* Date:    16 July 2002
* SUMMARY: Testing that we don't crash on Array.sort()
* See http://bugzilla.mozilla.org/show_bug.cgi?id=157652
*
*/
//-----------------------------------------------------------------------------
var bug = 157652;
var summary = "Testing that we don't crash on Array.sort()";

printBugNumber(bug);
printStatus(summary);

/*
 * As of 2002-08-13, the JS shell runs out of memory
 * when trying to sort such large arrays.
 *
 * We only want to test that we don't CRASH on the sort.
 * We'll be happy if we get the JS "out of memory" error.
 * Note this terminates the test with exit code 1 in Rhino
 * and exit code 3 in SpiderMonkey.
 *
 * Therefore we include |expectExitCode(n = 1,3)| below.
 *
 * The only problem will arise when the JS shell ever DOES
 * have enough memory to do the sort. Then this test will
 * terminate with the normal exit code 0 and fail.
 *
 * Right now, I can't see any other way to do this, because
 * "out of memory" is not a catchable error, i.e. it cannot
 * be caught with try...catch.
 */
if (inRhino())
  expectExitCode(1);
else
  expectExitCode(3);


/*
 * ECMA-262 Ed.3 Final, Section 15.4.2.2 : new Array(len)
 *
 * This states that |len| must be a a uint32 (unsigned 32-bit integer).
 * Note the UBound for uint32's is 2^32 -1 = 0xFFFFFFFF = 4,294,967,295.
 *
 * js> var arr = new Array(0xFFFFFFFF)
 * js> arr.length
 * 4294967295
 *
 * js> var arr = new Array(0x100000000)
 * RangeError: invalid array length
 *
 */
var a1 = Array(0x40000000);
a1.sort();
a1=null;


/*
 * Let's try another one -
 */
var a2=Array(0x10000000/4);
a2.sort();
a2=null;


/*
 * Let's try the biggest possible one (see above)
 */
var a3=Array(0xFFFFFFFF);
a3.sort();
a3 = null;
