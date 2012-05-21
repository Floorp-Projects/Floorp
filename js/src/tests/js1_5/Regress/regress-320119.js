// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 320119;
var summary = 'delegating objects and arguments, arity, caller, name';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

printStatus('original test');

function origtest(name, bar)
{
  this.name = name;
  this.bar = bar;
}

function Monty(id, name, bar)
{
  this.id = id;
  this.base = origtest;
  this.base(name, bar);
}

Monty.prototype = origtest;

function testtwo(notNamedName, bar)
{
  this.name = notNamedName;
  this.bar = bar;
}

function Python(id, notNamedName, bar)
{
  this.id = id;
  this.base = origtest;
  this.base(notNamedName, bar);
}

Python.prototype = testtwo;

var foo = new Monty(1, 'my name', 'sna!');

var manchu = new Python(1, 'my name', 'sna!');

printStatus('foo.name: ' + foo.name);
printStatus('manchu.name: ' + manchu.name);

expect = 'my name:my name';
actual = foo.name + ':' + manchu.name;
reportCompare(expect, actual, summary + ': override function..name');

// end original test

printStatus('test shared properties');

function testshared()
{
}

expect = false;
actual = testshared.hasOwnProperty('arguments');
reportCompare(expect, actual, summary + ': arguments no longer shared');

expect = false;
actual = testshared.hasOwnProperty('caller');
reportCompare(expect, actual, summary + ': caller no longer shared');

expect = false;
actual = testshared.hasOwnProperty('arity');
reportCompare(expect, actual, summary + ': arity no longer shared');

expect = false;
actual = testshared.hasOwnProperty('name');
reportCompare(expect, actual, summary + ': name no longer shared');

expect = true;
actual = testshared.hasOwnProperty('length');
reportCompare(expect, actual, summary + ': length still shared');

printStatus('test overrides');

function Parent()
{
  this.arguments = 'oarguments';
  this.caller = 'ocaller';
  this.arity = 'oarity';
  this.length = 'olength';
  this.name = 'oname';
}

function Child()
{
  this.base = Parent;
  this.base();
}

Child.prototype = Parent;

Child.prototype.value = function()
{
  return this.arguments + ',' + this.caller + ',' + this.arity + ',' +
  this.length + ',' + this.name;
};

var child = new Child();

expect = 'oarguments,ocaller,oarity,0,oname';
actual = child.value();
reportCompare(expect, actual, summary + ': overrides');
