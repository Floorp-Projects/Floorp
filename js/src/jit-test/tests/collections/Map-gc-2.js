// Check marking through the values of a Map.

load(libdir + "referencesVia.js");

var m = new Map;
for (var i = 0; i < 20; i++) {
    var n = new Map;
    n.set(i, m);
    assertEq(referencesVia(n, 'value', m), true);
    m = n;
}

gc();
gc();
