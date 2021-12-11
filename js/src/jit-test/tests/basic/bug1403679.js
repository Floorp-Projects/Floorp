load(libdir + "asserts.js");

const thisGlobal = this;
const otherGlobalSameCompartment = newGlobal({sameCompartmentAs: thisGlobal});
const otherGlobalNewCompartment = newGlobal({newCompartment: true});

const globals = [thisGlobal, otherGlobalSameCompartment, otherGlobalNewCompartment];

function testWithOptions(fn, variants = [undefined]) {
    for (let variant of variants) {
        for (let global of globals) {
            for (let options of [
                {},
                {proxy: true},
                {object: new FakeDOMObject()},
            ]) {
                fn(options, global, variant);
            }
        }
    }
}

function testWithGlobals(fn) {
    for (let global of globals) {
        fn(global);
    }
}

function testBasic(options, global) {
    let {object: source, transplant} = transplantableObject(options);

    // Validate that |source| is an object and |transplant| is a function.
    assertEq(typeof source, "object");
    assertEq(typeof transplant, "function");

    // |source| is created in the current global.
    assertEq(objectGlobal(source), this);

    // |source|'s prototype is %ObjectPrototype%, unless it's a FakeDOMObject.
    let oldPrototype;
    if (options.object) {
        oldPrototype = FakeDOMObject.prototype;
    } else {
        oldPrototype = Object.prototype;
    }
    assertEq(Object.getPrototypeOf(source), oldPrototype);

    // Properties can be created on |source|.
    assertEq(source.foo, undefined);
    source.foo = 1;
    assertEq(source.foo, 1);

    // Calling |transplant| transplants the object and then returns undefined.
    assertEq(transplant(global), undefined);

    // |source| was moved into the new global. If the new global is in a
    // different compartment, |source| is a now a CCW.
    if (global !== otherGlobalNewCompartment) {
        assertEq(objectGlobal(source), global);
    } else {
        assertEq(objectGlobal(source), null);
        assertEq(isProxy(source), true);
    }

    // The properties are copied over to the swapped object.
    assertEq(source.foo, 1);

    // The prototype was changed to %ObjectPrototype% of |global| or the
    // FakeDOMObject.prototype.
    let newPrototype;
    if (options.object) {
        newPrototype = global.FakeDOMObject.prototype;
    } else {
        newPrototype = global.Object.prototype;
    }
    assertEq(Object.getPrototypeOf(source), newPrototype);
}
testWithOptions(testBasic);

// Objects can be transplanted multiple times between globals.
function testTransplantMulti(options, global1, global2) {
    let {object: source, transplant} = transplantableObject(options);

    transplant(global1);
    transplant(global2);
}
testWithOptions(testTransplantMulti, globals);

// Test the case when the source object already has a wrapper in the target global.
function testHasWrapperInTarget(options, global) {
    let {object: source, transplant} = transplantableObject(options);

    // Create a wrapper for |source| in the other global.
    global.p = source;
    assertEq(global.eval("p"), source);

    if (options.proxy) {
        // It's a proxy object either way.
        assertEq(global.eval("isProxy(p)"), true);
    } else {
        if (global === otherGlobalNewCompartment) {
            // |isProxy| returns true because |p| is a CCW.
            assertEq(global.eval("isProxy(p)"), true);
        } else {
            // |isProxy| returns false because |p| is not a CCW.
            assertEq(global.eval("isProxy(p)"), false);
        }
    }

    // And now transplant it into that global.
    transplant(global);

    assertEq(global.eval("p"), source);

    if (options.proxy) {
        // It's a proxy object either way.
        assertEq(global.eval("isProxy(p)"), true);
    } else {
        // The previous CCW was replaced with a same-compartment object.
        assertEq(global.eval("isProxy(p)"), false);
    }
}
testWithOptions(testHasWrapperInTarget);

// Test the case when the source object has a wrapper, but in a different compartment.
function testHasWrapperOtherCompartment(options, global) {
    let thirdGlobal = newGlobal({newCompartment: true});
    let {object: source, transplant} = transplantableObject(options);

    // Create a wrapper for |source| in the new global.
    thirdGlobal.p = source;
    assertEq(thirdGlobal.eval("p"), source);

    // And now transplant the object.
    transplant(global);

    assertEq(thirdGlobal.eval("p"), source);
}
testWithOptions(testHasWrapperOtherCompartment);

// Ensure a transplanted object is correctly handled by (weak) collections.
function testCollections(options, global, AnySet) {
    let {object, transplant} = transplantableObject(options);

    let set = new AnySet();

    assertEq(set.has(object), false);
    set.add(object);
    assertEq(set.has(object), true);

    transplant(global);

    assertEq(set.has(object), true);
}
testWithOptions(testCollections, [Set, WeakSet]);

// Ensure DOM object slot is correctly transplanted.
function testDOMObjectSlot(global) {
    let domObject = new FakeDOMObject();
    let expectedValue = domObject.x;
    assertEq(typeof expectedValue, "number");

    let {object, transplant} = transplantableObject({object: domObject});
    assertEq(object, domObject);

    transplant(global);

    assertEq(object, domObject);
    assertEq(domObject.x, expectedValue);
}
testWithGlobals(testDOMObjectSlot);

function testArgumentValidation() {
    // Throws an error if too many arguments are present.
    assertThrowsInstanceOf(() => transplantableObject(thisGlobal, {}), Error);

    let {object, transplant} = transplantableObject();

    // Throws an error if called with no arguments.
    assertThrowsInstanceOf(() => transplant(), Error);

    // Throws an error if called with too many arguments.
    assertThrowsInstanceOf(() => transplant(thisGlobal, {}), Error);

    // Throws an error if the first argument isn't an object
    assertThrowsInstanceOf(() => transplant(null), Error);

    // Throws an error if the argument isn't a global object.
    assertThrowsInstanceOf(() => transplant({}), Error);

    // Throws an error if the 'object' option isn't a FakeDOMObject.
    assertThrowsInstanceOf(() => transplant({object: null}), Error);
    assertThrowsInstanceOf(() => transplant({object: {}}), Error);
}
testArgumentValidation();
