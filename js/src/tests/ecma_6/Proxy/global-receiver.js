// The global object can be the receiver passed to the get and set traps of a Proxy.

var global = this;
var proto = Object.getPrototypeOf(global);
var gets = 0, sets = 0;
Object.setPrototypeOf(global, new Proxy(proto, {
    has(t, id) {
        return id === "bareword" || id in t;
    },
    get(t, id, r) {
        gets++;
        assertEq(r, global);
        return t[id];  // wrong receiver
    },
    set(t, id, v, r) {
        sets++;
        assertEq(r, global);
        t[id] = v;  // wrong receiver
        return true;
    }
}));

assertEq(bareword, undefined);
assertEq(gets, 1);

bareword = 12;
assertEq(sets, 1);
assertEq(global.bareword, 12);

reportCompare(0, 0);
