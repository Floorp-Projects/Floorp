// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var x = {}, h = new WeakMap;
h.set(x, null);
gc();

reportCompare(0, 0, 'ok');
