// |reftest| skip -- bug xxx - reftest hang, BigO
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 335700;
var summary = 'Object Construction with getter closures should be O(N)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
test('Object', Object);
test('ObjectWithFunction', ObjectWithFunction);
test('ObjectWithGetter', ObjectWithGetter);

function test(desc, ctor)
{
  var start = 00000;
  var stop  = 40000;
  var incr  = (stop - start)/10;

  desc = summary + ': ' + desc;

  var data = {X:[], Y:[]};
  for (var i = start; i <= stop; i += incr)
  { 
    data.X.push(i);
    data.Y.push(runStart(ctor, i));
    gc();
  }

  var order = BigO(data);

  var msg = '';
  for (var p = 0; p < data.X.length; p++)
  {
    msg += '(' + data.X[p] + ', ' + data.Y[p] + '); ';
  }

  print(msg);
 
  reportCompare(true, order < 2, desc + ': BigO ' + order + ' < 2');

}

function ObjectWithFunction()
{
  var m_var = null;
  this.init = function(p_var) {
    m_var = p_var;
  }
  this.getVar = function()
    {
      return m_var;
    }
  this.setVar = function(v)
    {
      m_var = v;
    }
}
function ObjectWithGetter()
{
  var m_var = null;
  this.init = function(p_var) {
    m_var = p_var;
  }
  this.Var getter = function() {
    return m_var;
  }
  this.Var setter = function(v) {
    m_var = v;
  }
}

function runStart(ctor, limit)
{
  var arr = [];
  var start = Date.now();

  for (var i=0; i < limit; i++) {
    var obj = new ctor();
    obj.Var = 42;
    arr.push(obj);
  }

  var end = Date.now();
  var diff = end - start;

  return diff;
}
