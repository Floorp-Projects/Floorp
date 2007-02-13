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
	var TITLE = 'String:concat';

	writeHeaderToLog('Executing script: concat.js');
	writeHeaderToLog( SECTION + " "+ TITLE);

	var testcases = new getTestCases();
    test();
    
function getTestCases() {    

    var array = new Array();
    var item = 0;
    
	var aString = new String("test string");
	var bString = new String(" another ");

    array[item++] = new TestCase( SECTION, "String.prototype.concat.length", 0,     String.prototype.concat.length);

    array[item++] = new TestCase( SECTION, "aString.concat(' more')", "test string more",     aString.concat(' more').toString());
    array[item++] = new TestCase( SECTION, "aString.concat(bString)", "test string another ", aString.concat(bString).toString());
    array[item++] = new TestCase( SECTION, "aString                ", "test string",          aString.toString());
    array[item++] = new TestCase( SECTION, "bString                ", " another ",            bString.toString());
    array[item++] = new TestCase( SECTION, "aString.concat(345)    ", "test string345",       aString.concat(345).toString());
    array[item++] = new TestCase( SECTION, "aString.concat(true)   ", "test stringtrue",      aString.concat(true).toString());
    array[item++] = new TestCase( SECTION, "aString.concat(null)   ", "test stringnull",      aString.concat(null).toString());
    array[item++] = new TestCase( SECTION, "aString.concat([])     ", "test string",          aString.concat([]).toString());
    array[item++] = new TestCase( SECTION, "aString.concat([1,2,3])", "test string1,2,3",     aString.concat([1,2,3]).toString());

    array[item++] = new TestCase( SECTION, "'abcde'.concat(' more')", "abcde more",     'abcde'.concat(' more').toString());
    array[item++] = new TestCase( SECTION, "'abcde'.concat(bString)", "abcde another ", 'abcde'.concat(bString).toString());
    array[item++] = new TestCase( SECTION, "'abcde'                ", "abcde",          'abcde');
    array[item++] = new TestCase( SECTION, "'abcde'.concat(345)    ", "abcde345",       'abcde'.concat(345).toString());
    array[item++] = new TestCase( SECTION, "'abcde'.concat(true)   ", "abcdetrue",      'abcde'.concat(true).toString());
    array[item++] = new TestCase( SECTION, "'abcde'.concat(null)   ", "abcdenull",      'abcde'.concat(null).toString());
    array[item++] = new TestCase( SECTION, "'abcde'.concat([])     ", "abcde",          'abcde'.concat([]).toString());
    array[item++] = new TestCase( SECTION, "'abcde'.concat([1,2,3])", "abcde1,2,3",     'abcde'.concat([1,2,3]).toString());
    array[item++] = new TestCase( SECTION, "'abcde'.concat([1,2,3])", "abcde1,2,33,4,5string12345nulltrueundefined",     'abcde'.concat([1,2,3],[3,4,5],'string',12345,null,true,undefined).toString());

	//what should this do:
    array[item++] = new TestCase( SECTION, "'abcde'.concat()       ", "abcde",          'abcde'.concat().toString());

    //concat method transferred to other objects for use as method
   
    var myobj = new Object();
       
    myobj.concat = String.prototype.concat;
       
       
    array[item++] = new TestCase( SECTION, "myobj.concat([1,2,3])", "[object Object]1,2,33,4,5string12345nulltrueundefined",     myobj.concat([1,2,3],[3,4,5],'string',12345,null,true,undefined).toString());
    
    return array;
}
