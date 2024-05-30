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

// Since all the Array methods go through ArraySpeciesCreate, only testing concat below, but
// it should work for all classes. Testing will be hard in one file however due to the
// CacheIR optimization.
class Sentinel { };

class MyArray2 extends Array {
    static get [Symbol.species]() {
        return Sentinel;
    }
}

function array_concat() {
    let x = Array.from([1, 2]);
    let y = Array.from([3, 4]);
    let concat = x.concat(y);
    assertEq(concat instanceof Array, true);
}

function array_concat_subclassing_type_iii() {
    let x = MyArray2.from([1, 2]);
    let y = MyArray2.from([3, 4]);
    let concat = x.concat(y);
    assertEq(concat instanceof Sentinel, true);
}

test_function_for_use_counter_integration(array_concat, "SubclassingArrayTypeIII", /* expected_growth = */ false);
test_function_for_use_counter_integration(array_concat_subclassing_type_iii, "SubclassingArrayTypeIII", /* expected_growth = */ true);
