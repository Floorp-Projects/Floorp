// When assigning to an object with a proxy is on the prototype chain,
// the proxy's set handler is called.

var C = {};
var B = new Proxy(C, {
    get() { throw "FAIL"; },
    getOwnPropertyDescriptor() { throw "FAIL"; },
    has() { throw "FAIL"; },
    defineProperty() { throw "FAIL"; },
    set(target, id, value, receiver) {
        hits++;
        assertEq(target, C);
        assertEq(id, "x");
        assertEq(value, 3);
        assertEq(receiver, A);
        return true;
    }
});
var A = Object.create(B);

var hits = 0;
A.x = 3;
assertEq(hits, 1);
