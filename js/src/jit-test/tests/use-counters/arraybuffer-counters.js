function test_function_for_use_counter_integration(fn, counter, expected_growth = true) {
    let before = getUseCounterResults();
    assertEq(counter in before, true);

    fn();

    let after = getUseCounterResults();
    if (expected_growth) {
        printErr("Yes Increase: Before ", before[counter], " After", after[counter]);
        assertEq(after[counter] > before[counter], true);
    } else {
        printErr("No Increase: Before ", before[counter], " After", after[counter]);
        assertEq(after[counter] == before[counter], true);
    }
}

class Sentinel extends ArrayBuffer { }
class SharedSentinel extends SharedArrayBuffer { }

class MyArrayBuffer extends ArrayBuffer {
    static get [Symbol.species]() {
        return Sentinel;
    }
}

class MySharedArrayBuffer extends SharedArrayBuffer {
    static get [Symbol.species]() {
        return SharedSentinel;
    }
}

function array_buffer_slice() {
    let ab = new ArrayBuffer(16)
    let res = ab.slice(4, 12);
    assertEq(res instanceof ArrayBuffer, true);
}

function array_buffer_slice_type_iii() {
    let ab = new MyArrayBuffer(16)
    let res = ab.slice(4, 12);
    assertEq(res instanceof Sentinel, true);
}

test_function_for_use_counter_integration(array_buffer_slice, "SubclassingArrayBufferTypeIII", false);
test_function_for_use_counter_integration(array_buffer_slice_type_iii, "SubclassingArrayBufferTypeIII", true);

function shared_array_buffer_slice() {
    let ab = new SharedArrayBuffer(16)
    let res = ab.slice(4, 12);
    assertEq(res instanceof SharedArrayBuffer, true);
}

function shared_array_buffer_slice_type_iii() {
    let ab = new MySharedArrayBuffer(16)
    let res = ab.slice(4, 12);
    assertEq(res instanceof SharedSentinel, true);
}

test_function_for_use_counter_integration(shared_array_buffer_slice, "SubclassingSharedArrayBufferTypeIII", false);
test_function_for_use_counter_integration(shared_array_buffer_slice_type_iii, "SubclassingSharedArrayBufferTypeIII", true);
