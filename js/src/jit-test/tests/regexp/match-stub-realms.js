// Ensure regexp match stub uses the correct realm for the match object and
// the regexp statics.
function test() {
    var g1 = newGlobal({sameCompartmentAs: this});
    var g2 = newGlobal({sameCompartmentAs: this});
    g1.evaluate("function match(s) { return /(.)([\\d]+)/.exec(s); }");
    g2.evaluate("function match(s) { return /(.)([\\d]+)/.exec(s); }");
    for (var i = 0; i < 25; i++) {
        var res1 = g1.match(`A${i}`);
        var res2 = g2.match(`B${i}`);
        assertEq(objectGlobal(res1), g1);
        assertEq(objectGlobal(res2), g2);
        assertEq(g1.RegExp.$1, "A");
        assertEq(g1.RegExp.$2, String(i));
        assertEq(g2.RegExp.$1, "B");
        assertEq(g2.RegExp.$2, String(i));
    }
}
test();
