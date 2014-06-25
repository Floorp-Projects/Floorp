/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// See http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Guide:Iterators_and_Generators

//-----------------------------------------------------------------------------
var BUGNUMBER  = "410725";
var summary = "Test of the global Iterator constructor";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function iteratorToArray(iterator) {
  var result = [];
  for (var i in iterator) {
    result[result.length] = i;
  }
  return result.sort();
}

var obj = {a:1, b:2};

reportCompare('["a", "b"]',
              uneval(iteratorToArray(obj)),
              'uneval(iteratorToArray(obj))');

reportCompare('[["a", 1], ["b", 2]]',
              uneval(iteratorToArray(Iterator(obj))),
              'uneval(iteratorToArray(Iterator(obj)))');

reportCompare('[["a", 1], ["b", 2]]',
              uneval(iteratorToArray(new Iterator(obj))),
              'uneval(iteratorToArray(new Iterator(obj)))');

reportCompare('[["a", 1], ["b", 2]]',
              uneval(iteratorToArray(Iterator(obj,false))),
              'uneval(iteratorToArray(Iterator(obj,false)))');

reportCompare('[["a", 1], ["b", 2]]',
              uneval(iteratorToArray(new Iterator(obj,false))),
              'uneval(iteratorToArray(new Iterator(obj,false)))');

reportCompare('["a", "b"]',
              uneval(iteratorToArray(Iterator(obj,true))),
              'uneval(iteratorToArray(Iterator(obj,true)))');

reportCompare('["a", "b"]',
              uneval(iteratorToArray(new Iterator(obj,true))),
              'uneval(iteratorToArray(new Iterator(obj,true)))');

var flag;
var obji = {a:1, b:2};
obji.__iterator__ = function (b) { flag = b; yield -1; yield -2; }

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(obji)),
              'uneval(iteratorToArray(obji))');
reportCompare(true, flag, 'uneval(iteratorToArray(obji)) flag');

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(Iterator(obji))),
              'uneval(iteratorToArray(Iterator(obji)))');
reportCompare(false, flag, 'uneval(iteratorToArray(Iterator(obji))) flag');

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(new Iterator(obji))),
              'uneval(iteratorToArray(new Iterator(obji)))');
reportCompare(false, flag, 'uneval(iteratorToArray(new Iterator(obji))) flag');

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(Iterator(obji,false))),
              'uneval(iteratorToArray(Iterator(obji,false)))');
reportCompare(false, flag, 'uneval(iteratorToArray(Iterator(obji,false))) flag');

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(new Iterator(obji,false))),
              'uneval(iteratorToArray(new Iterator(obji,false)))');
reportCompare(false, flag, 'uneval(iteratorToArray(new Iterator(obji,false))) flag');

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(Iterator(obji,true))),
              'uneval(iteratorToArray(Iterator(obji,true)))');
reportCompare(true, flag, 'uneval(iteratorToArray(Iterator(obji,true))) flag');

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(new Iterator(obji,true))),
              'uneval(iteratorToArray(new Iterator(obji,true)))');
reportCompare(true, flag, 'uneval(iteratorToArray(new Iterator(obji,true))) flag');
