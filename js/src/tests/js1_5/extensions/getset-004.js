/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 14 April 2001
 *
 * SUMMARY: Testing  obj.__defineSetter__(), obj.__defineGetter__()
 * Note: this is a non-ECMA language extension
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing  obj.__defineSetter__(), obj.__defineGetter__()';
var statprefix = 'Status: ';
var status = '';
var statusitems = [ ];
var actual = '';
var actualvalues = [ ];
var expect= '';
var expectedvalues = [ ];
var cnDEFAULT = 'default name';
var cnFRED = 'Fred';
var obj = {};
var obj2 = {};
var s = '';


// SECTION1: define getter/setter directly on an object (not its prototype)
obj = new Object();
obj.nameSETS = 0;
obj.nameGETS = 0;
obj.__defineSetter__('name', function(newValue) {this._name=newValue; this.nameSETS++;});
obj.__defineGetter__('name', function() {this.nameGETS++; return this._name;});

status = 'In SECTION1 of test after 0 sets, 0 gets';
actual = [obj.nameSETS,obj.nameGETS];
expect = [0,0];
addThis();

s = obj.name;
status = 'In SECTION1 of test after 0 sets, 1 get';
actual = [obj.nameSETS,obj.nameGETS];
expect = [0,1];
addThis();

obj.name = cnFRED;
status = 'In SECTION1 of test after 1 set, 1 get';
actual = [obj.nameSETS,obj.nameGETS];
expect = [1,1];
addThis();

obj.name = obj.name;
status = 'In SECTION1 of test after 2 sets, 2 gets';
actual = [obj.nameSETS,obj.nameGETS];
expect = [2,2];
addThis();


// SECTION2: define getter/setter in Object.prototype
Object.prototype.nameSETS = 0;
Object.prototype.nameGETS = 0;
Object.prototype.__defineSetter__('name', function(newValue) {this._name=newValue; this.nameSETS++;});
Object.prototype.__defineGetter__('name', function() {this.nameGETS++; return this._name;});

obj = new Object();
status = 'In SECTION2 of test after 0 sets, 0 gets';
actual = [obj.nameSETS,obj.nameGETS];
expect = [0,0];
addThis();

s = obj.name;
status = 'In SECTION2 of test after 0 sets, 1 get';
actual = [obj.nameSETS,obj.nameGETS];
expect = [0,1];
addThis();

obj.name = cnFRED;
status = 'In SECTION2 of test after 1 set, 1 get';
actual = [obj.nameSETS,obj.nameGETS];
expect = [1,1];
addThis();

obj.name = obj.name;
status = 'In SECTION2 of test after 2 sets, 2 gets';
actual = [obj.nameSETS,obj.nameGETS];
expect = [2,2];
addThis();


// SECTION 3: define getter/setter in prototype of user-defined constructor
function TestObject()
{
}
TestObject.prototype.nameSETS = 0;
TestObject.prototype.nameGETS = 0;
TestObject.prototype.__defineSetter__('name', function(newValue) {this._name=newValue; this.nameSETS++;});
TestObject.prototype.__defineGetter__('name', function() {this.nameGETS++; return this._name;});
TestObject.prototype.name = cnDEFAULT;

obj = new TestObject();
status = 'In SECTION3 of test after 1 set, 0 gets'; // (we set a default value in the prototype)
actual = [obj.nameSETS,obj.nameGETS];
expect = [1,0];
addThis();

s = obj.name;
status = 'In SECTION3 of test after 1 set, 1 get';
actual = [obj.nameSETS,obj.nameGETS];
expect = [1,1];
addThis();

obj.name = cnFRED;
status = 'In SECTION3 of test after 2 sets, 1 get';
actual = [obj.nameSETS,obj.nameGETS];
expect = [2,1];
addThis();

obj.name = obj.name;
status = 'In SECTION3 of test after 3 sets, 2 gets';
actual = [obj.nameSETS,obj.nameGETS];
expect = [3,2];
addThis();

obj2 = new TestObject();
status = 'obj2 = new TestObject() after 1 set, 0 gets';
actual = [obj2.nameSETS,obj2.nameGETS];
expect = [1,0]; // we set a default value in the prototype -
addThis();

// Use both obj and obj2  -
obj2.name = obj.name +  obj2.name;
status = 'obj2 = new TestObject() after 2 sets, 1 get';
actual = [obj2.nameSETS,obj2.nameGETS];
expect = [2,1];
addThis();

status = 'In SECTION3 of test after 3 sets, 3 gets';
actual = [obj.nameSETS,obj.nameGETS];
expect = [3,3];  // we left off at [3,2] above -
addThis();


//---------------------------------------------------------------------------------
test();
//---------------------------------------------------------------------------------


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual.toString();
  expectedvalues[UBound] = expect.toString();
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], getStatus(i));
  }

  exitFunc ('test');
}


function getStatus(i)
{
  return statprefix + statusitems[i];
}
