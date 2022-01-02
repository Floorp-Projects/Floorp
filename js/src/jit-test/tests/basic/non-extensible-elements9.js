function testNonExtensibleStoreFallibleT() {
    // Create an array with initialized-length = capacity = 2.
    var x = [8, 0];

    // Make it non-extensible.
    Object.preventExtensions(x);

    // Now reduce initialized-length by one, so that initialized-length < capacity is true.
    x.length = 1;

    // There's enough capacity in the elements storage to save the new element,
    // but we still need to reject the store since the object is non-extensible.
    x[1] = 4;

    assertEq(x.length, 1);
    assertEq(x[0], 8);
}

for (var i = 0; i < 15; ++i)
    testNonExtensibleStoreFallibleT();

// Repeat testNonExtensibleStoreFallibleT for the MIRType::Value specialization.
function testNonExtensibleStoreFallibleV(i) {
    // Create an array with initialized-length = capacity = 2.
    var x = [8, ""];

    // Make it non-extensible.
    Object.preventExtensions(x);

    // Now reduce initialized-length by one, so that initialized-length < capacity is true.
    x.length = 1;

    // There's enough capacity in the elements storage to save the new element,
    // but we still need to reject the store since the object is non-extensible.
    x[1] = [true, 1][i & 1];

    assertEq(x.length, 1);
    assertEq(x[0], 8);
}

for (var i = 0; i < 15; ++i)
    testNonExtensibleStoreFallibleV(i);

function testForInIterationNonExtensibleStoreFallibleT() {
    // Create an array with initialized-length = capacity = 2.
    var x = [8, 0];

    // Make it non-extensible.
    Object.preventExtensions(x);

    // Modifying an array's length takes a different path during for-in
    // iteration of the array.
    for (var k in x) {
        // Now reduce initialized-length by one, so that initialized-length < capacity is true.
        x.length = 1;
    }

    // There's enough capacity in the elements storage to save the new element,
    // but we still need to reject the store since the object is non-extensible.
    x[1] = 4;

    assertEq(x.length, 1);
    assertEq(x[0], 8);
}

for (var i = 0; i < 15; ++i)
    testForInIterationNonExtensibleStoreFallibleT();

// Repeat testForInIterationNonExtensibleStoreFallibleT for the MIRType::Value specialization.
function testForInIterationNonExtensibleStoreFallibleV(i) {
    // Create an array with initialized-length = capacity = 2.
    var x = [8, ""];

    // Make it non-extensible.
    Object.preventExtensions(x);

    // Modifying an array's length takes a different path during for-in
    // iteration of the array.
    for (var k in x) {
        // Now reduce initialized-length by one, so that initialized-length < capacity is true.
        x.length = 1;
        break;
    }

    // There's enough capacity in the elements storage to save the new element,
    // but we still need to reject the store since the object is non-extensible.
    x[1] = [true, 1][i & 1];

    assertEq(x.length, 1);
    assertEq(x[0], 8);
}

for (var i = 0; i < 15; ++i)
    testForInIterationNonExtensibleStoreFallibleV(i);

function testNonExtensibleArrayPop() {
    // Create an array with initialized-length = capacity = 2.
    var x = [8, 0];

    // Make it non-extensible.
    Object.preventExtensions(x);

    // Now reduce initialized-length by one, so that initialized-length < capacity is true.
    x.pop();

    // There's enough capacity in the elements storage to save the new element,
    // but we still need to reject the store since the object is non-extensible.
    x[1] = 4;

    assertEq(x.length, 1);
    assertEq(x[0], 8);
}

for (var i = 0; i < 15; ++i)
    testNonExtensibleArrayPop();

function testNonExtensibleArrayPopNonWritable() {
    // Create an array with initialized-length = capacity = 2.
    var x = [8, 0];

    // Make it non-extensible.
    Object.preventExtensions(x);

    // And make the "length" property non-writable.
    Object.defineProperty(x, "length", {writable: false});

    // Now reduce initialized-length by one, so that initialized-length < capacity is true.
    // This doesn't update |x.length|, because the "length" property is non-writable.
    try {
        x.pop();
    } catch {}

    // There's enough capacity in the elements storage to save the new element,
    // but we still need to reject the store since the object is non-extensible.
    for (var i = 0; i < 100; ++i)
        x[1] = 4;

    assertEq(x.length, 2);
    assertEq(x[0], 8);
    assertEq(x[1], undefined);
    assertEq(1 in x, false);
}

for (var i = 0; i < 15; ++i)
    testNonExtensibleArrayPopNonWritable();

function testNonExtensibleArrayShift() {
    // Create an array with initialized-length = capacity = 2.
    var x = [8, 0];

    // Make it non-extensible.
    Object.preventExtensions(x);

    // Now reduce initialized-length by one, so that initialized-length < capacity is true.
    x.shift();

    // There's enough capacity in the elements storage to save the new element,
    // but we still need to reject the store since the object is non-extensible.
    x[1] = 4;

    assertEq(x.length, 1);
    assertEq(x[0], 0);
}

for (var i = 0; i < 15; ++i)
    testNonExtensibleArrayShift();

function testNonExtensibleArraySplice() {
    // Create an array with initialized-length = capacity = 2.
    var x = [8, 0];

    // Make it non-extensible.
    Object.preventExtensions(x);

    // Now reduce initialized-length by one, so that initialized-length < capacity is true.
    x.splice(1, 1);

    // There's enough capacity in the elements storage to save the new element,
    // but we still need to reject the store since the object is non-extensible.
    x[1] = 4;

    assertEq(x.length, 1);
    assertEq(x[0], 8);
}

for (var i = 0; i < 15; ++i)
    testNonExtensibleArraySplice();
