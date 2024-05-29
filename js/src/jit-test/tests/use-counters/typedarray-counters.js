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

const TYPED_ARRAY_CONSTRUCTORS = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array,
    BigInt64Array,
    BigUint64Array
]

const SUBCLASS_CONSTRUCTORS = TYPED_ARRAY_CONSTRUCTORS.map((x) => { let c = eval(`class ${x.name}Subclass extends ${x.name} {}; ${x.name}Subclass`); print(c); return c; });

const TYPED_ARRAY_FROM = TYPED_ARRAY_CONSTRUCTORS.map((C) => function () { let r = C.from([]); assertEq(r instanceof C, true); });
const SUBCLASS_FROM = SUBCLASS_CONSTRUCTORS.map((C) => function () { let r = C.from([]); assertEq(r instanceof C, true); });

TYPED_ARRAY_FROM.forEach((x) => {
    test_function_for_use_counter_integration(x, "SubclassingTypedArrayTypeII",  /* expected_growth = */ false)
})
SUBCLASS_FROM.forEach((x) => {
    test_function_for_use_counter_integration(x, "SubclassingTypedArrayTypeII",  /* expected_growth = */ true)
})

let source = [];
source[Symbol.iterator] = null;

const TYPED_ARRAY_FROM_NON_ITERATOR = TYPED_ARRAY_CONSTRUCTORS.map((C) => function () { let r = C.from([]); assertEq(r instanceof C, true); });
const SUBCLASS_FROM_NON_ITERATOR = SUBCLASS_CONSTRUCTORS.map((C) => function () { let r = C.from([]); assertEq(r instanceof C, true); });

TYPED_ARRAY_FROM_NON_ITERATOR.forEach((x) => {
    test_function_for_use_counter_integration(x, "SubclassingTypedArrayTypeII",  /* expected_growth = */ false)
})
SUBCLASS_FROM_NON_ITERATOR.forEach((x) => {
    test_function_for_use_counter_integration(x, "SubclassingTypedArrayTypeII",  /* expected_growth = */ true)
})

// Type III Subclassing
class Sentinel extends Int8Array { }
class MyInt8Array extends Int8Array {
    static get [Symbol.species]() {
        return Sentinel;
    }
}

function int8_test() {
    let a = Int8Array.of(0, 1, 2, 3);
    let r = a.filter(() => true);
    assertEq(r instanceof Int8Array, true);
}

function myint8_test() {
    let a = MyInt8Array.of(0, 1, 2, 3);
    let r = a.filter(() => true);
    assertEq(r instanceof Sentinel, true);
}

test_function_for_use_counter_integration(int8_test, "SubclassingTypedArrayTypeIII", /*expected_growth=*/ false);
test_function_for_use_counter_integration(myint8_test, "SubclassingTypedArrayTypeIII", /*expected_growth=*/ true);
