load(libdir + "asserts.js");

// Allow [[GetOwnPropertyDescriptor]] to spoof enumerability of target object's
// properties. Note that this also tests that the getOwnPropertyDescriptor is
// called by Object.keys(), as expected.

var target = {};
Object.defineProperty(target, "foo", { configurable: true, enumerable: false });

function getPD(target, P) {
    var targetDesc = Object.getOwnPropertyDescriptor(target, P);
    // Lie about enumerability
    targetDesc.enumerable = !targetDesc.enumerable;
    return targetDesc;
}

var proxy = new Proxy(target, { getOwnPropertyDescriptor: getPD });

assertDeepEq(Object.keys(proxy), ["foo"]);

Object.defineProperty(target, "foo", {configurable: true, enumerable: true});

assertDeepEq(Object.keys(proxy), []);
