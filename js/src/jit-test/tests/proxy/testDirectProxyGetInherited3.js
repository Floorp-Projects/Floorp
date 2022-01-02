// Recursion through the get hook works; runaway recursion is checked.

load(libdir + "asserts.js");

var hits = 0, limit = 10;
var proto = new Proxy({}, {
    get(t, id, r) {
        assertEq(r, obj);
        if (hits++ >= limit)
            return "ding";
        return obj[id];
    }
});

var obj = Object.create(proto);
assertEq(obj.prop, "ding");

hits = 0;
limit = Infinity;
assertThrowsInstanceOf(() => obj.prop, InternalError);
assertEq(hits > 10, true);
