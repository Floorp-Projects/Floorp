// Check marking through the keys of a Map.

load(libdir + "referencesVia.js");

var m = new Map;
for (var i = 0; i < 20; i++) {
    var n = new Map;
    n.set(m, i);
    assertEq(referencesVia(n, 'key', m), true);
    m = n;
}

gc();
gc();

// TODO: walk the chain using for-of to make sure everything is still there
