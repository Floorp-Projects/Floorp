setJitCompilerOption("ion.forceinlineCaches", 1);

let offsets = [213, 559, 255, 515, 30, 507, 252, 329, 487, 7];

function update_index(i, j) {
    var offset = offsets[j % offsets.length];
    return i + offset;
}

function compute_index(initial, count) {
    for (var i = 0; i < count; i++) {
        initial = update_index(initial, i);
    }
    return initial;
}

// This is written so that the IC added in the bug activates.
function mutate_array(array, count, epsilon = 0) {
    var index = 0;
    for (var i = 0; i < count; i++) {
        index = update_index(index, i);
        array[index] = i + epsilon;
    }
    return array[offsets[0]+offsets[1]] === (1 + epsilon) &&
           array[10] === undefined;
}

// Monomorphizing mutate_array to ensure we get the IC chains we want
function create_variant(variant) {
    var source = mutate_array.toString().replace("mutate_array", "mutate_array_"+variant);
    return source;
}

function test_basic() {
    eval(create_variant("basic"));
    var x = [];

    var count = 100;
    assertEq(mutate_array_basic(x, count), true);
    var end = compute_index(0, count);
    assertEq(x[end], count - 1);
    assertEq(x[end - 1], undefined);
}

// Ensure the IC respects frozen.
function test_frozen() {
    eval(create_variant("frozen"));
    var x = [];
    Object.freeze(x);

    var count = 100;
    assertEq(mutate_array_frozen(x, count), false);
    assertEq(x.length, 0);

    var end = compute_index(0, count);

    var y = [];
    assertEq(mutate_array_frozen(y, count), true);
    assertEq(y[end], count - 1);
    Object.freeze(y);

    // After a mutated array is frozen, can't subsequently modify elements
    assertEq(mutate_array_frozen(x, count, 10), false);
    assertEq(y[end], count - 1);
}

// Let's make sure updates to the array happen as expected.
function test_update() {
    eval(create_variant("update"));

    var x = [];
    var count = 100;
    assertEq(mutate_array_update(x, count), true);
    var end = compute_index(0, count);
    assertEq(x[end], count - 1);
    assertEq(x[end - 1], undefined);

    var epsilon = 2;
    mutate_array_update(x, 200, epsilon);
    assertEq(x[end], count -1 + epsilon)
}

// Elements may be non-writable, let us not write them.
function test_nonwritable() {
    eval(create_variant("nonwritable"));
    var x = [];
    var count = 100;
    var index = compute_index(0, 10);
    Object.defineProperty(x, index, {value: -10, writable: false});
    mutate_array_nonwritable(x, count);
    assertEq(x[index], -10);
}

// Random indices can get setters, let's make sure we honour those.
function test_setter() {
    eval(create_variant("setter"));
    var x = [];
    var count = 100;
    var index = compute_index(0, 80);
    var sigil = 0;
    Object.defineProperty(x, index, {set(newVal) {sigil++; }});
    mutate_array_setter(x, count);
    assertEq(sigil, 1);
    assertEq(x[index], undefined);
}

// Ensure indexes on the prototype don't break things;
//
function test_proto_indices() {
    eval(create_variant("proto_indices"));
    var x = [];
    var count = 100;
    var index = compute_index(0, 80);
    x.__proto__[index] = "hello";
    mutate_array_proto_indices(x, count);
    assertEq(x.__proto__[index], "hello");
    assertEq(x[index], 79);
}

test_basic();
test_frozen();
test_update();
test_nonwritable();
test_setter();
test_proto_indices();
