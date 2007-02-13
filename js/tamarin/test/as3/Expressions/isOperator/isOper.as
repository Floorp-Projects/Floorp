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

var SECTION = "Expressions";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS3";        // Version of ECMAScript or ActionScript 
var TITLE   = "is Operator";       // Provide ECMA section title or a description


startTest();                // leave this alone

//vars, functions and classes to be used by the test
import isOper.*;

var myClassA:TestClassA = new TestClassA();	// class TestClassA {}
var myClassB:TestClassB = new TestClassB();	// class TestClassB extends TestClassA {}
var myClassC:TestClassC = new TestClassC();	// class TestClassC extends TestClassB implements TestInterface {}
											// interface TestInterface {}
function emptyFunction() {};
var emptyAnyObj:* = new Object();
var undefinedVar;
var emptyObject:Object = new Object();
var myDate:Date = new Date(1977,8,24);
var filledArr:Array = [0,1,2,3,4,5,6,7,"eight",new Object(),Math.PI,9,10];
var emptyArr = [];


var valueArr:Array = [{},"string","10",null,undefined,true,false,0,1,-1,1.23,-1.23,NaN,Infinity,emptyFunction,emptyObject,
			myClassA, myClassB,myClassC,myDate,Number.MAX_VALUE,Number.MIN_VALUE,Number.NEGATIVE_INFINITY,Number.POSITIVE_INFINITY,
			uint.MAX_VALUE,uint.MIN_VALUE,int.MAX_VALUE,int.MIN_VALUE,"",emptyAnyObj,undefinedVar,filledArr,emptyArr];

// The valueDescArr array has the string representations of the valueArr values.
// This is due to the fact that some values (objects) do not resolve to strings.
var valueDescArr:Array = ["{}",'"string"','"10"',"null","undefined","true","false","0","1","-1","1.23","-1.23","NaN","Infinity","emptyFunction",
			"emptyObject","myClassA","myClassB","myClassC","myDate","Number.MAX_VALUE",'Number.MIN_VALUE','Number.NEGATIVE_INFINITY','Number.POSITIVE_INFINITY',
			'uint.MAX_VALUE','uint.MIN_VALUE','int.MAX_VALUE','int.MIN_VALUE','"" (empty string)',"empty * Obj","undefinedVar","filled Array","empty Array"];

var typeArr:Array =		[String,Number,int,uint,Boolean,Object,	Function,TestClassA,TestClassB,TestClassC,TestInterface,Date,Array];
var typeDescArr:Array =	["String","Number","int","uint","Boolean","Object","Function","TestClassA","TestClassB","TestClassC","TestInterface","Date","Array"];

// The resultArr Array holds the expected boolean values when each value is compared to type using "is"
var resultArr  = new Array(typeArr.length);

var x:int = 0;	//counter for resultArr.  DO NOT change the line order of the array.

//			[String,Number,	int,	uint,	Boolean,Object,	Function,TestClassA, TestClassB,TestClassC,	TestInterface,	Date,	Array];
resultArr[x++] =	[false,	false,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// {}
resultArr[x++] = 	[true,	false,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// "string"
resultArr[x++] = 	[true,	false,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// "10"
resultArr[x++] = 	[false,	false,	false,	false,	false,	false,	false,	    false,  false,		false,		false,	false,	false];		// null
resultArr[x++] = 	[false,	false,	false,	false,	false,	false,	false,	    false,  false,		false,		false,	false,	false];		// undefined
resultArr[x++] = 	[false,	false,	false,	false,	true,	true,	false,	    false,  false,		false,		false,	false,	false];		// true
resultArr[x++] = 	[false,	false,	false,	false,	true,	true,	false,	    false,  false,		false,		false,	false,	false];		// false
resultArr[x++] = 	[false,	true,	true,	true,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// 0
resultArr[x++] = 	[false,	true,	true,	true,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// 1
resultArr[x++] = 	[false,	true,	true,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// -1
resultArr[x++] =	[false,	true,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// 1.23
resultArr[x++] =	[false,	true,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// -1.23
resultArr[x++] =	[false,	true,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// NaN
resultArr[x++] =	[false,	true,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// Infinity
resultArr[x++] =	[false,	false,	false,	false,	false,	true,	true,	    false,  false,		false,		false,	false,	false];		// emptyFunction
resultArr[x++] =	[false,	false,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// emptyObject
resultArr[x++] =	[false,	false,	false,	false,	false,	true,	false,	    true,   false,		false,		false,	false,	false];		// myClassA
resultArr[x++] =	[false,	false,	false,	false,	false,	true,	false,	    true,   true,		false,		false,	false,	false];		// myClassB
resultArr[x++] =	[false,	false,	false,	false,	false,	true,	false,	    true,   true,		true,		true,	false,	false];		// myClassC
resultArr[x++] =	[false,	false,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	true,	false];		// myDate
resultArr[x++] =	[false,	true,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// Number.MAX_VALUE
resultArr[x++] =	[false,	true,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// Number.MIN_VALUE
resultArr[x++] =	[false,	true,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// Number.NEGATIVE_INFINITY
resultArr[x++] =	[false,	true,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// Number.POSITIVE_INFINITY
resultArr[x++] =	[false,	true,	false,	true,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// uint.MAX_VALUE
resultArr[x++] =	[false,	true,	true,	true,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// uint.MIN_VALUE
resultArr[x++] =	[false,	true,	true,	true,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// int.MAX_VALUE
resultArr[x++] =	[false,	true,	true,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// int.MIN_VALUE
resultArr[x++] = 	[true,	false,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// "" (empty string)
resultArr[x++] = 	[false,	false,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	false];		// emptyAnyObj
resultArr[x++] = 	[false,	false,	false,	false,	false,	false,	false,	    false,  false,		false,		false,	false,	false];		// undefinedVar
resultArr[x++] = 	[false,	false,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	true];		// filledArray
resultArr[x++] = 	[false,	false,	false,	false,	false,	true,	false,	    false,  false,		false,		false,	false,	true];		// emptyArray


var typeArrLength = typeArr.length;

for (var i:int = 0; i < valueArr.length; i++) {
	for (var j:int = 0; j < typeArrLength; j++) {
		AddTestCase(valueDescArr[i]+" is "+ typeDescArr[j],resultArr[i][j],(valueArr[i] is typeArr[j]));
	}
}

////////////////////////////////////////////////////////////////

test();       // leave this alone.  this executes the test cases and
              // displays results.
