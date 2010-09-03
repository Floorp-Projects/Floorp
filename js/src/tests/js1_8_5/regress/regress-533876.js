/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Gary Kwong and Jason Orendorff
 */

var savedEval = eval;
var x = [0];
eval();

x.__proto__ = this;  // x has non-dictionary scope
try {
    DIE;
} catch(e) {
}

delete eval;  // force dictionary scope for global
gc();
eval = savedEval;
var f = eval("(function () { return /x/; })");
x.watch('x', f);  // clone property from global to x, including SPROP_IN_DICTIONARY flag

reportCompare("ok", "ok", "bug 533876");
