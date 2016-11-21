// |jit-test| error: 42; need-for-each
load(libdir + "immutable-prototype.js");

// Suppress the large quantity of output on stdout (eg from calling
// dumpHeap()).
os.file.redirect(null);

var blacklist = {
    'quit': true,
    'crash': true,
    'readline': true,
    'terminate': true,
    'nestedShell': true,
};

function f(y) {}
for each(let e in newGlobal()) {
    if (e.name in blacklist)
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
    for each (b in arr) {
        if (b.name in blacklist)
            continue;
        try {
            f(b)
        } catch (e) {}
    }
})();

throw 42;
