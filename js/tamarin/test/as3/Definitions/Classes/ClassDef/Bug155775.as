/* ***** BEGIN LICENSE BLOCK ***** 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1 

The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
"License"); you may not use this file except in compliance with the License. You may obtain 
a copy of the License at http://www.mozilla.org/MPL/ 

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
language governing rights and limitations under the License. 

The Original Code is [Open Source Virtual Machine.] 

The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
by the Initial Developer are Copyright (C)[ 2005-2006 ] Adobe Systems Incorporated. All Rights 
Reserved. 

Contributor(s): Adobe AS3 Team

Alternatively, the contents of this file may be used under the terms of either the GNU 
General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
LGPL are applicable instead of those above. If you wish to allow use of your version of this 
file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
version of this file under the terms of the MPL, indicate your decision by deleting provisions 
above and replace them with the notice and other provisions required by the GPL or the 
LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
under the terms of any one of the MPL, the GPL or the LGPL. 

 ***** END LICENSE BLOCK ***** */
// Bug 155775: http://flashqa.macromedia.com/bugapp/detail.asp?ID=155775

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";               // Version of JavaScript or ECMA
var TITLE   = "Class Definition";  // Provide ECMA section title or a description
var BUGNUMBER = "155775";

startTest();
//--------------------------------------------------

dynamic class MyClass
{
	public var length:int;
	function MyClass() {
		length = 3;
		this[0] = 0;
		this[1] = 1;
		this[2] = 123;
	}

	function toString():String {
		var s:String = "";
		for (var i:int = 0; i < length; i++)
		{
			s += this[i]
			if (i < length - 1)
				s += ",";
		}
		return s;
	}
}
MyClass.prototype.concat = Array.prototype.concat;
MyClass.prototype.every = Array.prototype.every;
MyClass.prototype.filter = Array.prototype.filter;
MyClass.prototype.forEach = Array.prototype.forEach;
MyClass.prototype.indexOf = Array.prototype.indexOf;
MyClass.prototype.join = Array.prototype.join;
MyClass.prototype.lastIndexOf = Array.prototype.lastIndexOf;
MyClass.prototype.map = Array.prototype.map;
MyClass.prototype.pop = Array.prototype.pop;
MyClass.prototype.push = Array.prototype.push;
MyClass.prototype.reverse = Array.prototype.reverse;
MyClass.prototype.shift = Array.prototype.shift;
MyClass.prototype.slice = Array.prototype.slice;
MyClass.prototype.some = Array.prototype.some;
MyClass.prototype.sort = Array.prototype.sort;
MyClass.prototype.sortOn = Array.prototype.sortOn;
MyClass.prototype.splice = Array.prototype.splice;
MyClass.prototype.unshift = Array.prototype.unshift;

class Vegetable {
    public var name:String;
    public var price:Number;

    public function Vegetable(name:String, price:Number) {
        this.name = name;
        this.price = price;
    }

    public function toString():String {
        return " " + name + ":" + price;
    }
}

function isNumeric(item:*, index:int, array:Array):Boolean {
	return (item is Number);
}

var a:MyClass = new MyClass();
var b:Array = new Array(0, 1, 123);

// These currently throw a Type Error when -AS3 is not used.
try {
	thisError = "no error";
	AddTestCase("a.every(isNumeric)", b.every(isNumeric), a.every(isNumeric));
	AddTestCase("a.filter(isNumeric)", b.filter(isNumeric), a.filter(isNumeric));
	AddTestCase("a.forEach(isNumeric)", b.forEach(isNumeric), a.forEach(isNumeric));
	AddTestCase("a.map(isNumeric)", b.map(isNumeric), a.map(isNumeric));
	AddTestCase("a.some(isNumeric)", b.some(isNumeric), a.some(isNumeric));
} catch(e:TypeError) {
	thisError = e.message;
} finally {
	AddTestCase("Array methods with callback functions", "no error", typeError(thisError));
}

AddTestCase("a.concat(b)", b.concat(b), a.concat(b));
AddTestCase("a.indexOf(0)", b.indexOf(0), a.indexOf(0));
AddTestCase("a.indexOf(1)", b.indexOf(1), a.indexOf(1));
AddTestCase("a.indexOf(123)", b.indexOf(123), a.indexOf(123));
AddTestCase("a.join(' and ')", b.join(" and "), a.join(" and "));

AddTestCase("a.lastIndexOf(0)", b.lastIndexOf(0), a.lastIndexOf(0));
AddTestCase("a.lastIndexOf(1)", b.lastIndexOf(1), a.lastIndexOf(1));
AddTestCase("a.lastIndexOf(123)", b.lastIndexOf(123), a.lastIndexOf(123));

AddTestCase("a.pop()", b.pop(), a.pop());
AddTestCase("a.push(123)", b.push(123), a.push(123));
AddTestCase("a.reverse()", b.reverse().toString(), a.reverse().toString());
AddTestCase("a.shift(123)", b.shift(123), a.shift(123));
AddTestCase("a.slice(0, 2)", b.slice(0, 2).toString(), a.slice(0, 2).toString());

var aSort:MyClass = new MyClass();
aSort.pop();
aSort.pop();
aSort.pop();
aSort.push("lettuce");
aSort.push("spinach");
aSort.push("asparagus");
aSort.push("celery");
aSort.push("squash");
var bSort:Array = new Array();
bSort.push("lettuce");
bSort.push("spinach");
bSort.push("asparagus");
bSort.push("celery");
bSort.push("squash");
AddTestCase("aSort.sort()", bSort.sort().toString(), aSort.sort().toString());

var aSortOn:MyClass = new MyClass();
aSortOn.pop();
aSortOn.pop();
aSortOn.pop();
aSortOn.push(new Vegetable("lettuce", 1.49));
aSortOn.push(new Vegetable("spinach", 1.89));
aSortOn.push(new Vegetable("asparagus", 3.99));
aSortOn.push(new Vegetable("celery", 1.29));
aSortOn.push(new Vegetable("squash", 1.44));
var bSortOn:Array = new Array();
bSortOn.push(new Vegetable("lettuce", 1.49));
bSortOn.push(new Vegetable("spinach", 1.89));
bSortOn.push(new Vegetable("asparagus", 3.99));
bSortOn.push(new Vegetable("celery", 1.29));
bSortOn.push(new Vegetable("squash", 1.44));
AddTestCase("aSortOn.sortOn('name')", bSortOn.sortOn("name").toString(), aSortOn.sortOn("name").toString());

// print results after splicing since splice() returns array of spliced elements, not the result of splicing
AddTestCase("a.splice(2, 0, 3)", (b.splice(2, 0, 3), b.toString()), (a.splice(2, 0, 3), a.toString()));

AddTestCase("a.unshift(0)", b.unshift(0), a.unshift(0));

//--------------------------------------------------
test();
