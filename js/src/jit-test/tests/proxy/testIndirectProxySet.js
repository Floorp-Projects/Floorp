// BaseProxyHandler::set correctly defines a property on receiver if the
// property being assigned doesn't exist anywhere on the proto chain.

var p = Proxy.create({
    getOwnPropertyDescriptor() { return undefined; },
    getPropertyDescriptor() { return undefined; },
    defineProperty() { throw "FAIL"; }
});
var q = new Proxy(p, {
    defineProperty(t, id, desc) {
        assertEq(t, p);
        assertEq(id, "x");
        assertEq(desc.configurable, true);
        assertEq(desc.enumerable, true);
        assertEq(desc.writable, true);
        assertEq(desc.value, 3);
        hits++;
        return true;
    }
});
var hits = 0;

// This assignment eventually reaches ScriptedIndirectProxyHandler::set
// with arguments (proxy=p, receiver=q). Test that it finishes by defining
// a property on receiver, not on proxy.
q.x = 3;
assertEq(hits, 1);
