function f() {
    var g = newGlobal({sameCompartmentAs: this});
    g.evaluate(`function f() {}`);
    var boundF = g.f.bind(null);
    for (var i = 0; i < 50; i++) {
        var obj = new boundF();
        assertEq(objectGlobal(obj), g);
    }
}
f();
