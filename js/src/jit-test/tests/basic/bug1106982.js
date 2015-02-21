var x = "wrong";
var t = {x: "x"};
var p = new Proxy(t, {
    has(t, id) {
        var found = id in t;
        delete t[id];
        return found;
    },
    get(t, id) { return t[id]; }
});
with (p)
    x += " x";
assertEq(t.x, "undefined x");
