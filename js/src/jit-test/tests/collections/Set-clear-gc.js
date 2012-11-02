// Clearing a Set removes any strong references to its elements.

load(libdir + "referencesVia.js");

var s = Set();
var obj = {};
s.add(obj);
assertEq(referencesVia(s, "key", obj), true);
s.clear();
if (typeof findReferences == 'function')
    assertEq(referencesVia(s, "key", obj), false);
