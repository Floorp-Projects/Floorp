/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    03 June 2002
 * SUMMARY: Function param or local var with same name as a function property
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=137000
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=138708
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=150032
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=150859
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 137000;
var summary = 'Function param or local var with same name as a function prop';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Note use of 'x' both for the parameter to f,
 * and as a property name for |f| as an object
 */
function f(x)
{
}

status = inSection(1);
f.x = 12;
actual = f.x;
expect = 12;
addThis();



/*
 * A more elaborate example, using the call() method
 * to chain constructors from child to parent.
 *
 * The key point is the use of the same name 'p' for both
 * the parameter to the constructor, and as a property name
 */
function parentObject(p)
{
  this.p = 1;
}

function childObject()
{
  parentObject.call(this);
}
childObject.prototype = parentObject;

status = inSection(2);
var objParent = new parentObject();
actual = objParent.p;
expect = 1;
addThis();

status = inSection(3);
var objChild = new childObject();
actual = objChild.p;
expect = 1;
addThis();



/*
 * A similar set-up. Here the same name is being used for
 * the parameter to both the Base and Child constructors,
 */
function Base(id)
{
}

function Child(id)
{
  this.prop = id;
}
Child.prototype=Base;

status = inSection(4);
var c1 = new Child('child1');
actual = c1.prop;
expect = 'child1';
addThis();



/*
 * Use same identifier as a property name, too -
 */
function BaseX(id)
{
}

function ChildX(id)
{
  this.id = id;
}
ChildX.prototype=BaseX;

status = inSection(5);
c1 = new ChildX('child1');
actual = c1.id;
expect = 'child1';
addThis();



/*
 * From http://bugzilla.mozilla.org/show_bug.cgi?id=150032
 *
 * Here the same name is being used both for a local variable
 * declared in g(), and as a property name for |g| as an object
 */
function g()
{
  var propA = g.propA;
  var propB = g.propC;

  this.getVarA = function() {return propA;}
  this.getVarB = function() {return propB;}
}
g.propA = 'A';
g.propB = 'B';
g.propC = 'C';
var obj = new g();

status = inSection(6);
actual = obj.getVarA(); // this one was returning 'undefined'
expect = 'A';
addThis();

status = inSection(7);
actual = obj.getVarB(); // this one is easy; it never failed
expect = 'C';
addThis();



/*
 * By martin.honnen@gmx.de
 * From http://bugzilla.mozilla.org/show_bug.cgi?id=150859
 *
 * Here the same name is being used for a local var in F
 * and as a property name for |F| as an object
 *
 * Twist: the property is added via another function.
 */
function setFProperty(val)
{
  F.propA = val;
}

function F()
{
  var propA = 'Local variable in F';
}

status = inSection(8);
setFProperty('Hello');
actual = F.propA; // this was returning 'undefined'
expect = 'Hello';
addThis();




//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
