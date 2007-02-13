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

	var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
	var VERSION = 'no version';
    startTest();
	var TITLE = 'String:push,splice,concat,unshift,sort';

	writeHeaderToLog('Executing script: general3.as');
	writeHeaderToLog( SECTION + " "+ TITLE);

	var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

	var array1 = new Array();
	var array2 = [];
    var array5 = [0,1,2,3,4,5,6,7,8,9];
    var array6 = [9,8,7,6,5,4,3,2,1,0];
	var size   = 10;

        for (var i=0; i < size; i++)
        {
            array1[i] = i;
            array2[i] = size-1-i;
        }


	// the following for loop reverses the order of array1 so
	// that it should be similarly ordered to array2
	var array3;
	for (i = array1.length; i > 0; i--)
	{
		array3 = array1.slice(1,i);
		array1.splice(1,i-1);
		array1 = array3.concat(array1);
	}

        // the following for loop reverses the order of array1 so
	// that it should be similarly ordered to array2
	var array3;
	for (i = array5.length; i > 0; i--)
	{
		array7 = array5.slice(1,i);
		array5.splice(1,i-1);
		array5 = array7.concat(array5);
	}

	// the following for loop reverses the order of array1
	// and array2
	for (i = 0; i < size; i++)
	{
	    array1.push(array1.shift());
	    array2.unshift(array2.pop());
	}

        // the following for loop reverses the order of array5
	// and array6
	for (i = 0; i < size; i++)
	{
	    array5.push(array5.shift());
	    array6.unshift(array6.pop());
	}

	array[item++] = new TestCase( SECTION, "Array.push,pop,shift,unshift,slice,splice", true,String(array1) == String(array2));
        array[item++] = new TestCase( SECTION, "Array.push,pop,shift,unshift,slice,splice", true,String(array5) == String(array6));
	array1.sort();
	array2.sort();
        array5.sort();
        array6.sort();
	array[item++] = new TestCase( SECTION, "Array.sort", true,String(array1) == String(array2));
        array[item++] = new TestCase( SECTION, "Array.sort", true,String(array5) == String(array6));

    return ( array );
}
