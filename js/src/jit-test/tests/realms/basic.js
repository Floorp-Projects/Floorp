var g1 = newGlobal({sameCompartmentAs: this});
var g2 = newGlobal({sameCompartmentAs: g1});
g2.x = g1;
gc();
assertEq(objectGlobal(Math), this);
assertEq(objectGlobal(g1.print), g1);
assertEq(objectGlobal(g2.x), g1);

// Different-compartment realms have wrappers.
assertEq(objectGlobal(newGlobal().Math), null);

function testCrossRealmProto() {
    var g = newGlobal({sameCompartmentAs:this});

    for (var i = 0; i < 20; i++) {
        var o = Object.create(g.Math);
        assertEq(objectGlobal(o), this);
        assertEq(o.__proto__, g.Math);

        var a = Reflect.construct(Array, [], g.Object);
        assertEq(Array.isArray(a), true);
        assertEq(objectGlobal(a), this);
        assertEq(a.__proto__, g.Object.prototype);
    }
}
testCrossRealmProto();
