load(libdir + "asserts.js");

// Allow [[GetOwnPropertyDescriptor]] to spoof enumerability of target object's
// properties. Note that this also tests that the getOwnPropertyDescriptor is
// called by Object.keys(), as expected.

var target = {};
var handler = {
    getOwnPropertyDescriptor : function (target, P) {
        var targetDesc = Object.getOwnPropertyDescriptor(target, P);
        // Lie about enumerability
        targetDesc.enumerable = !targetDesc.enumerable;
        return targetDesc;
    }
};

for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
    Object.defineProperty(target, "foo", { configurable: true, enumerable: false });
    assertDeepEq(Object.keys(p), ["foo"]);

    Object.defineProperty(target, "foo", {configurable: true, enumerable: true});
    assertDeepEq(Object.keys(p), []);
}
