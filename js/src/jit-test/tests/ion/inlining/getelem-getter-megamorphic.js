// Test for inlined getters for jsop_getelem accesses, where the getter is a
// property on the prototype and a megamorphic IC is attached.

function makeObjects(name) {
    class Base {
        constructor(v) {
            this._prop = v;
        }
        get [name]() {
            return this._prop;
        }
    }

    // When we hit |TYPE_FLAG_OBJECT_COUNT_LIMIT|, the objects are marked as
    // |TYPE_FLAG_ANYOBJECT|. That means less than |TYPE_FLAG_OBJECT_COUNT_LIMIT|
    // objects need to be created to have no unknown objects in the type set.
    const TYPE_FLAG_OBJECT_COUNT_LIMIT = 7;

    // |ICState::ICState::MaxOptimizedStubs| defines the maximum number of
    // optimized stubs, so as soon as we hit the maximum number, the megamorphic
    // state is entered.
    const ICState_MaxOptimizedStubs = 6;

    // Create enough classes to enter megamorphic state, but not too much to
    // have |TYPE_FLAG_ANYOBJECT| in the TypeSet.
    const OBJECT_COUNT = Math.min(ICState_MaxOptimizedStubs, TYPE_FLAG_OBJECT_COUNT_LIMIT);

    var objects = [];
    for (var i = 0; i < OBJECT_COUNT; ++i) {
        objects.push(new class extends Base {}(1));
    }

    return objects;
}

// Defined outside of the test functions to ensure they're recognised as
// constants in Ion.
var atom = "prop";
var symbol = Symbol();

function testAtom() {
    var objects = makeObjects(atom);

    function f() {
        var actual = 0;
        var expected = 0;
        for (var i = 0; i < 1000; i++) {
            var obj = objects[i % objects.length];
            actual += obj[atom];
            expected += obj._prop;
        }
        assertEq(actual, expected);
    }

    for (var i = 0; i < 2; ++i) {
        f();
    }
}
testAtom();

function testSymbol() {
    var objects = makeObjects(symbol);

    function f() {
        var actual = 0;
        var expected = 0;
        for (var i = 0; i < 1000; i++) {
            var obj = objects[i % objects.length];
            actual += obj[symbol];
            expected += obj._prop;
        }
        assertEq(actual, expected);
    }

    for (var i = 0; i < 2; ++i) {
        f();
    }
}
testSymbol();
