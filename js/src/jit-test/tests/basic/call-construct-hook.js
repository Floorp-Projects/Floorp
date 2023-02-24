// Tests for invoking JSClass call/construct hooks.

function testBasic() {
    let obj = {fun: newObjectWithCallHook()};
    for (var i = 0; i < 1000; i++) {
        let res = obj.fun(1, 2, 3);
        assertEq(res.this, obj);
        assertEq(res.callee, obj.fun);
        assertEq(JSON.stringify(res.arguments), "[1,2,3]");
        assertEq(res.newTarget, undefined);

        res = new obj.fun(1, 2, 3);
        assertEq(res.this, "<is_constructing>");
        assertEq(res.callee, obj.fun);
        assertEq(JSON.stringify(res.arguments), "[1,2,3]");
        assertEq(res.newTarget, obj.fun);
    }
}
testBasic();

function testSpread() {
    let obj = {fun: newObjectWithCallHook()};
    for (var i = 0; i < 1000; i++) {
        let res = obj.fun(...[1, 2, 3]);
        assertEq(res.this, obj);
        assertEq(res.callee, obj.fun);
        assertEq(JSON.stringify(res.arguments), "[1,2,3]");
        assertEq(res.newTarget, undefined);

        res = new obj.fun(...[1, 2, 3]);
        assertEq(res.this, "<is_constructing>");
        assertEq(res.callee, obj.fun);
        assertEq(JSON.stringify(res.arguments), "[1,2,3]");
        assertEq(res.newTarget, obj.fun);
    }
}
testSpread();

function testNewTarget() {
    let obj = newObjectWithCallHook();
    let newTargetVal = function() {};
    for (var i = 0; i < 1000; i++) {
        let res = Reflect.construct(obj, [], newTargetVal);
        assertEq(res.this, "<is_constructing>");
        assertEq(res.callee, obj);
        assertEq(res.arguments.length, 0);
        assertEq(res.newTarget, newTargetVal);
    }
}
testNewTarget();

function testRealmSwitch() {
    // We currently switch to the callable's realm for class hook calls (similar to
    // JS function calls). This might change at some point, but for now assert the
    // returned object's realm matches that of the callee.

    let g = newGlobal({sameCompartmentAs: this});
    let obj = g.newObjectWithCallHook();
    assertEq(objectGlobal(obj), g);

    for (var i = 0; i < 1000; i++) {
        let res = obj(1, 2);
        assertEq(objectGlobal(res), g);
        assertEq(res.this, undefined);
        assertEq(res.callee, obj);
        assertEq(JSON.stringify(res.arguments), "[1,2]");

        res = new obj(1, 2);
        assertEq(objectGlobal(res), g);
        assertEq(res.this, "<is_constructing>");
        assertEq(res.callee, obj);
        assertEq(JSON.stringify(res.arguments), "[1,2]");
        assertEq(res.newTarget, obj);
    }
}
testRealmSwitch();

function testCrossCompartmentWrapper() {
    // Call/construct hooks can be called through cross-compartment wrappers.

    let g = newGlobal({newCompartment: true});
    let obj = g.newObjectWithCallHook();
    assertEq(isProxy(obj), true); // It's a CCW.

    for (var i = 0; i < 1000; i++) {
        let res = obj(1, 2);
        assertEq(isProxy(res), true);
        assertEq(res.this, undefined);
        assertEq(res.callee, obj);
        assertEq(JSON.stringify(res.arguments), "[1,2]");

        res = new obj(1, 2);
        assertEq(isProxy(res), true);
        assertEq(res.this, "<is_constructing>");
        assertEq(res.callee, obj);
        assertEq(JSON.stringify(res.arguments), "[1,2]");
        assertEq(res.newTarget, obj);
    }
}
testCrossCompartmentWrapper();
