/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Robert Sayre
 */


//-----------------------------------------------------------------------------
var BUGNUMBER = 366941;
var summary = 'Destructuring enumerations, iterations';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var list1 = [[1,2],[3,4],[5,6]];
  var list2 = [[1,2,3],[4,5,6],[7,8,9]];

  expect = '1,2;3,4;5,6;';
  actual = '';

  for each (var [foo, bar] in list1) {
    actual += foo + "," + bar + ";";
  }

  reportCompare(expect, actual, summary + ': 1');

  expect = '1,2,3;4,5,6;7,8,9;';
  actual = '';
  for each (var [foo, bar, baz] in list2) {
    actual += foo + "," + bar + "," + baz + ";";
  }

  reportCompare(expect, actual, summary + ': 2');

  function gen(list) {
    for each (var test in list) {
      yield test;
    }
  }

  var iter1 = gen(list1);

  expect = '1,2;3,4;5,6;';
  actual = '';

  for (var [foo, bar] in iter1) {
    actual += foo + "," + bar + ";";
  }

  reportCompare(expect, actual, summary + ': 3');

  var iter2 = gen(list2);
  expect = '1,2,3;4,5,6;7,8,9;';
  actual = '';

  for (var [foo, bar, baz] in iter2) {
    actual += foo + "," + bar + "," + baz + ";";
  }

  reportCompare(expect, actual, summary + ': 4');

  exitFunc ('test');
}
