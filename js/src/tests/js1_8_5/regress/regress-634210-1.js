/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function f() {}
f.p = function() {};
Object.freeze(f);
f.p;

reportCompare(0, 0, "ok");
