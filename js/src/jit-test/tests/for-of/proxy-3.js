// An exception thrown from a proxy trap while getting the .iterator method is propagated.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var p = new Proxy({}, {
    get(target, property) {
        if (property === Symbol.iterator)
            throw "fit";
        return undefined;
    }
});
assertThrowsValue(function () { for (var v of p) {} }, "fit");
