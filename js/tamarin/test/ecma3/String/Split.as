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

	var SECTION = '15.5.4.14';
	var VERSION = 'no version';

	startTest();
	var TITLE = 'String:split';

	writeHeaderToLog('Executing script: split.as');
	writeHeaderToLog( SECTION + " "+ TITLE);

	var count = 0;
	var testcases = new Array();

	var bString = new String("one,two,three,four,five");

	splitString = bString.split(",");
	testcases[count++] = new TestCase( SECTION, "String.split(\",\")", "one", splitString[0]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\",\")", "two", splitString[1]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\",\")", "three", splitString[2]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\",\")", "four", splitString[3]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\",\")", "five", splitString[4]);

	bString = new String("one two three four five");
	splitString = bString.split(" ");

	testcases[count++] = new TestCase( SECTION, "String.split(\" \")", "one", splitString[0]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\" \")", "two", splitString[1]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\" \")", "three", splitString[2]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\" \")", "four", splitString[3]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\" \")", "five", splitString[4]);

	bString = new String("one,two,three,four,five");
	splitString = bString.split();
	
	testcases[count++] = new TestCase( SECTION, "bString.split()", "one,two,three,four,five", splitString[0]);
	testcases[count++] = new TestCase( SECTION, "bString.split()", "one,two,three,four,five", 
splitString.toString());
	
	bString = new String("one two three");
	splitString = bString.split("");

	testcases[count++] = new TestCase( SECTION, "bString.split(\"\")", "o", splitString[0]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\"\")", "n", splitString[1]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\"\")", "r", splitString[10]);
	testcases[count++] = new TestCase( SECTION, "bString.split(\"\")", "e", splitString[12]);

	bString = new String("one-1,two-2,four-4");
	regExp = /,/;
	splitString = bString.split(regExp);
	
	testcases[count++] = new TestCase( SECTION, "bString.split(regExp)", "one-1", splitString[0]);
	testcases[count++] = new TestCase( SECTION, "bString.split(regExp)", "two-2", splitString[1]);
	testcases[count++] = new TestCase( SECTION, "bString.split(regExp)", "four-4", splitString[2]);

	
	function test()
	{
	   for ( tc=0; tc < testcases.length; tc++ ) {
	        testcases[tc].passed = writeTestCaseResult(
	        testcases[tc].expect,
	        testcases[tc].actual,
	        testcases[tc].description +" = "+
	        testcases[tc].actual );
	        testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
	   }
	   stopTest();
	   return ( testcases );
	}

	test();

