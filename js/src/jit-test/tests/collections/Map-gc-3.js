// Maps do not keep deleted keys or values alive.

load(libdir + "referencesVia.js");

var m = new Map;
var k = {}, v = {};
m.set(k, v);
m.delete(k);
if (typeof findReferences == 'function') {
    assertEq(referencesVia(m, 'key', k), false);
    assertEq(referencesVia(m, 'value', v), false);
}
