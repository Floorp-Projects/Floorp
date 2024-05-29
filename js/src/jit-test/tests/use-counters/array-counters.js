function test_function_for_use_counter_integration(fn, counter, expected_growth = true) {
    let before = getUseCounterResults();
    assertEq(counter in before, true);

    fn();

    let after = getUseCounterResults();
    if (expected_growth) {
        console.log("Yes Increase: Before ", before[counter], " After", after[counter]);
        assertEq(after[counter] > before[counter], true);
    } else {
        console.log("No Increase: Before ", before[counter], " After", after[counter]);
        assertEq(after[counter] == before[counter], true);
    }
}

class MyArray extends Array { }

function array_from() {
    let r = Array.from([1, 2, 3]);
    assertEq(r instanceof Array, true);
}

function array_from_subclassing_type_ii() {
    assertEq(MyArray.from([1, 2, 3]) instanceof MyArray, true);
}

test_function_for_use_counter_integration(array_from, "SubclassingArrayTypeII", /* expected_growth = */ false);
test_function_for_use_counter_integration(array_from_subclassing_type_ii, "SubclassingArrayTypeII", /* expected_growth = */ true);

function array_of() {
    let r = Array.of([1, 2, 3]);
    assertEq(r instanceof Array, true);
}

function array_of_subclassing_type_ii() {
    assertEq(MyArray.of([1, 2, 3]) instanceof MyArray, true);
}

test_function_for_use_counter_integration(array_of, "SubclassingArrayTypeII", /* expected_growth = */ false);
test_function_for_use_counter_integration(array_of_subclassing_type_ii, "SubclassingArrayTypeII", /* expected_growth = */ true);


// Array.fromAsync
function array_fromAsync() {
    let r = Array.fromAsync([1, 2, 3]);
    r.then((x) => assertEq(x instanceof Array, true));
}

function array_fromAsync_subclassing_type_ii() {
    MyArray.fromAsync([1, 2, 3]).then((x) => assertEq(x instanceof MyArray, true));
}

test_function_for_use_counter_integration(array_fromAsync, "SubclassingArrayTypeII", /* expected_growth = */ false);
test_function_for_use_counter_integration(array_fromAsync_subclassing_type_ii, "SubclassingArrayTypeII", /* expected_growth = */ true);

// Array.of
