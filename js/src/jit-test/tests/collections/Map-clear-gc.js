// Clearing a Map removes any strong references to its keys and values.

load(libdir + "referencesVia.js");

var m = Map();
var k = {}, v = {};
m.set(k, v);
assertEq(referencesVia(m, "key", k), true);
assertEq(referencesVia(m, "value", v), true);
m.clear();
if (typeof findReferences == 'function') {
    assertEq(referencesVia(m, "key", k), false);
    assertEq(referencesVia(m, "value", v), false);
}
