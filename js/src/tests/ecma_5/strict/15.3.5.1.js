/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function fn() {
  return function(a, b, c) { };
}

assertEq(testLenientAndStrict('var f = fn(); f.length = 1; f.length',
                              returns(3), raisesException(TypeError)),
         true);

reportCompare(true, true);
