var x = "wrong";
var t = {x: "x"};
var hits = 0;
var p = new Proxy(t, {
    has(t, id) {
        var found = id in t;
        if (++hits == 2)
            delete t[id];
        return found;
    },
    get(t, id) { return t[id]; }
});
with (p)
    x += " x";
// If you change this testcase (e.g. because we fix the number of
// has() calls we end up making to match spec better), don't forget to
// update bug1106982-2.js too.  See also bug 1145641.
assertEq(hits, 2);
assertEq(t.x, "undefined x");
