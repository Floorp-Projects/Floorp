// |jit-test| error: 42; skip-if: getBuildConfiguration("wasi")
load(libdir + "immutable-prototype.js");

// Suppress the large quantity of output on stdout (eg from calling
// dumpHeap()).
os.file.redirect(null);

var ignorelist = {
    'quit': true,
    'crash': true,
    'readline': true,
    'terminate': true,
    'nukeAllCCWs': true,
    'cacheIRHealthReport': true,
    'getInnerMostEnvironmentObject': true,
    'getEnclosingEnvironmentObject': true,
    'getEnvironmentObjectType' : true,
};

function f(y) {}
for (let e of Object.values(newGlobal())) {
    if (e.name in ignorelist)
	continue;
    print(e.name);
    try {
        e();
    } catch (r) {}
}
(function() {
    arguments;
    if (globalPrototypeChainIsMutable())
        Object.prototype.__proto__ = newGlobal()
    function f(y) {
        y()
    }
    var arr = [];
    arr.__proto__ = newGlobal();
    for (b of Object.values(arr)) {
        if (b.name in ignorelist)
            continue;
        try {
            f(b)
        } catch (e) {}
    }
})();

throw 42;
