function testRedefineGetter() {
    var callGetter = function(o) {
        return o.x;
    };

    var proto = {get foo() {}, bar: 1};
    var obj = Object.create(proto);

    // Add "x" getter on the prototype. Warm up the IC.
    var count1 = 0;
    Object.defineProperty(proto, "x", {get: function(v) {
        count1++;
    }, configurable: true});
    for (var i = 0; i < 20; i++) {
        callGetter(obj);
    }
    assertEq(count1, 20);

    // Redefine "x" with a different getter. Ensure the new getter is called.
    var count2 = 0;
    Object.defineProperty(proto, "x", {get: function() {
        count2++;
    }, configurable: true});
    for (var i = 0; i < 20; i++) {
        callGetter(obj);
    }
    assertEq(count1, 20);
    assertEq(count2, 20);
}
testRedefineGetter();

function testRedefineSetter() {
    var callSetter = function(o) {
        o.x = 1;
    };

    var proto = {get foo() {}, bar: 1};
    var obj = Object.create(proto);

    // Add "x" setter on the prototype. Warm up the IC.
    var count1 = 0;
    Object.defineProperty(proto, "x", {set: function(v) {
        count1++;
    }, configurable: true});
    for (var i = 0; i < 20; i++) {
        callSetter(obj);
    }
    assertEq(count1, 20);

    // Redefine "x" with a different setter. Ensure the new setter is called.
    var count2 = 0;
    Object.defineProperty(proto, "x", {set: function() {
        count2++;
    }, configurable: true});
    for (var i = 0; i < 20; i++) {
        callSetter(obj);
    }
    assertEq(count1, 20);
    assertEq(count2, 20);
}
testRedefineSetter();

function testDeleteAdd() {
    var callGetter = function(o) {
        return o.x;
    };

    var proto = {get foo() {}, bar: 1};
    var obj = Object.create(proto);

    // Add "x" getter on the prototype. Warm up the IC.
    var count1 = 0;
    Object.defineProperty(proto, "x", {get: function() {
        count1++;
    }, configurable: true});
    for (var i = 0; i < 20; i++) {
        callGetter(obj);
    }
    assertEq(count1, 20);

    // Delete the getter.
    delete proto.x;

    // Add "x" back with a different getter. Ensure the new getter is called.
    var count2 = 0;
    Object.defineProperty(proto, "x", {get: function() {
        count2++;
    }, configurable: true});
    for (var i = 0; i < 20; i++) {
        callGetter(obj);
    }
    assertEq(count1, 20);
    assertEq(count2, 20);
}
testDeleteAdd();

function testAccessorToDataAndBack() {
    var callGetter = function(o) {
        return o.x;
    };

    var proto = {get foo() {}, bar: 1};
    var obj = Object.create(proto);

    // Add "x" getter on the prototype. Warm up the IC.
    var count1 = 0;
    Object.defineProperty(proto, "x", {get: function() {
        count1++;
    }, configurable: true});
    for (var i = 0; i < 20; i++) {
        callGetter(obj);
    }
    assertEq(count1, 20);

    // Turn the getter into a data property.
    Object.defineProperty(proto, "x", {configurable: true, value: 123});

    // Turn the data property into a (different) getter. Ensure the new getter is
    // called.
    var count2 = 0;
    Object.defineProperty(proto, "x", {get: function() {
        count2++;
    }, configurable: true});
    for (var i = 0; i < 20; i++) {
        callGetter(obj);
    }
    assertEq(count1, 20);
    assertEq(count2, 20);
}
testAccessorToDataAndBack();
