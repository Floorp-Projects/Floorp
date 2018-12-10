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

function testSystemNonSystemRealms() {
    var systemRealm = newGlobal({systemPrincipal: true});
    var ex;
    try {
        var nonSystemRealm = newGlobal({sameCompartmentAs: systemRealm, principal: 10});
    } catch(e) {
        ex = e;
    }
    assertEq(ex.toString().includes("non-system realms"), true);
    ex = null;
    try {
        systemRealm = newGlobal({systemPrincipal: true, sameCompartmentAs: this});
    } catch(e) {
        ex = e;
    }
    assertEq(ex.toString().includes("non-system realms"), true);
}
testSystemNonSystemRealms();

function testNewObjectCache() {
    // NewObjectCache lookup based on the proto should not return a cross-realm
    // object.
    var g = newGlobal({sameCompartmentAs: this});
    var o1 = g.evaluate("Object.create(Math)");
    var o2 = Object.create(g.Math);
    assertEq(objectGlobal(o1), g);
    assertEq(objectGlobal(o2), this);
}
testNewObjectCache();
