/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// delete o[p] only performs ToString(p) once, even if there's a strict error.
var hits = 0;
var p = {
    toString: function () {
        hits++;
        return "noconfig";
    }
};
assertEq(testLenientAndStrict('var o = Object.freeze({noconfig: "ow"}); delete o[p]',
                              returns(false), raisesException(TypeError)),
         true);
assertEq(hits, 2);

reportCompare(true, true);
