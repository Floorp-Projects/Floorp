// The receiver argument is passed through prototype chains and proxies with no "set" handler.

var hits;
var a = new Proxy({}, {
    set(t, id, value, receiver) {
        assertEq(id, "prop");
        assertEq(value, 3);
        assertEq(receiver, b);
        hits++;
    }
});
var b = Object.create(Object.create(new Proxy(Object.create(new Proxy(a, {})), {})));
hits = 0;
b.prop = 3;
assertEq(hits, 1);
assertEq(b.prop, undefined);
