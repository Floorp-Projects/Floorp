/////////////////////////////////////////
// This is a generated file!
// See jit-tests/etc/generate-nosuchproperty-tests.js for the code
// that generated this code!
/////////////////////////////////////////

/////////////////////////////////////////
// PRELUDE                             //
/////////////////////////////////////////

function createTower(n) {
    var result = Object.create(null);
    for (var i = 0; i < n; i++)
        result = Object.create(result);
    return result;
}
function updateChain(obj, depth, prop, value) {
    // Walk down the proto chain |depth| iterations and set |prop| to |value|.
    var cur = obj;
    for (var i = 0; i < depth; i++)
        cur = Object.getPrototypeOf(cur);

    var desc = {value:value, writable:true, configurable:true, enumerable:true};
    Object.defineProperty(cur, prop, desc);
}

/////////////////////////////////////////
// TEST CASES                          //
/////////////////////////////////////////

//// Test chain of length 0 with late-property-addition at depth 0
function runChain_0_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_0_0() {
    var obj = createTower(0);
    assertEq(runChain_0_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_0_0(obj), 900);
}

//// Test chain of length 1 with late-property-addition at depth 0
function runChain_1_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_1_0() {
    var obj = createTower(1);
    assertEq(runChain_1_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_1_0(obj), 900);
}

//// Test chain of length 1 with late-property-addition at depth 1
function runChain_1_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_1_1() {
    var obj = createTower(1);
    assertEq(runChain_1_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_1_1(obj), 900);
}

//// Test chain of length 2 with late-property-addition at depth 0
function runChain_2_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_2_0() {
    var obj = createTower(2);
    assertEq(runChain_2_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_2_0(obj), 900);
}

//// Test chain of length 2 with late-property-addition at depth 1
function runChain_2_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_2_1() {
    var obj = createTower(2);
    assertEq(runChain_2_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_2_1(obj), 900);
}

//// Test chain of length 2 with late-property-addition at depth 2
function runChain_2_2(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_2_2() {
    var obj = createTower(2);
    assertEq(runChain_2_2(obj), NaN);
    updateChain(obj, 2, 'foo', 9);
    assertEq(runChain_2_2(obj), 900);
}

//// Test chain of length 3 with late-property-addition at depth 0
function runChain_3_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_3_0() {
    var obj = createTower(3);
    assertEq(runChain_3_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_3_0(obj), 900);
}

//// Test chain of length 3 with late-property-addition at depth 1
function runChain_3_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_3_1() {
    var obj = createTower(3);
    assertEq(runChain_3_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_3_1(obj), 900);
}

//// Test chain of length 3 with late-property-addition at depth 2
function runChain_3_2(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_3_2() {
    var obj = createTower(3);
    assertEq(runChain_3_2(obj), NaN);
    updateChain(obj, 2, 'foo', 9);
    assertEq(runChain_3_2(obj), 900);
}

//// Test chain of length 3 with late-property-addition at depth 3
function runChain_3_3(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_3_3() {
    var obj = createTower(3);
    assertEq(runChain_3_3(obj), NaN);
    updateChain(obj, 3, 'foo', 9);
    assertEq(runChain_3_3(obj), 900);
}

//// Test chain of length 4 with late-property-addition at depth 0
function runChain_4_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_4_0() {
    var obj = createTower(4);
    assertEq(runChain_4_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_4_0(obj), 900);
}

//// Test chain of length 4 with late-property-addition at depth 1
function runChain_4_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_4_1() {
    var obj = createTower(4);
    assertEq(runChain_4_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_4_1(obj), 900);
}

//// Test chain of length 4 with late-property-addition at depth 2
function runChain_4_2(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_4_2() {
    var obj = createTower(4);
    assertEq(runChain_4_2(obj), NaN);
    updateChain(obj, 2, 'foo', 9);
    assertEq(runChain_4_2(obj), 900);
}

//// Test chain of length 4 with late-property-addition at depth 3
function runChain_4_3(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_4_3() {
    var obj = createTower(4);
    assertEq(runChain_4_3(obj), NaN);
    updateChain(obj, 3, 'foo', 9);
    assertEq(runChain_4_3(obj), 900);
}

//// Test chain of length 4 with late-property-addition at depth 4
function runChain_4_4(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_4_4() {
    var obj = createTower(4);
    assertEq(runChain_4_4(obj), NaN);
    updateChain(obj, 4, 'foo', 9);
    assertEq(runChain_4_4(obj), 900);
}

//// Test chain of length 5 with late-property-addition at depth 0
function runChain_5_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_5_0() {
    var obj = createTower(5);
    assertEq(runChain_5_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_5_0(obj), 900);
}

//// Test chain of length 5 with late-property-addition at depth 1
function runChain_5_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_5_1() {
    var obj = createTower(5);
    assertEq(runChain_5_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_5_1(obj), 900);
}

//// Test chain of length 5 with late-property-addition at depth 2
function runChain_5_2(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_5_2() {
    var obj = createTower(5);
    assertEq(runChain_5_2(obj), NaN);
    updateChain(obj, 2, 'foo', 9);
    assertEq(runChain_5_2(obj), 900);
}

//// Test chain of length 5 with late-property-addition at depth 3
function runChain_5_3(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_5_3() {
    var obj = createTower(5);
    assertEq(runChain_5_3(obj), NaN);
    updateChain(obj, 3, 'foo', 9);
    assertEq(runChain_5_3(obj), 900);
}

//// Test chain of length 5 with late-property-addition at depth 4
function runChain_5_4(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_5_4() {
    var obj = createTower(5);
    assertEq(runChain_5_4(obj), NaN);
    updateChain(obj, 4, 'foo', 9);
    assertEq(runChain_5_4(obj), 900);
}

//// Test chain of length 5 with late-property-addition at depth 5
function runChain_5_5(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_5_5() {
    var obj = createTower(5);
    assertEq(runChain_5_5(obj), NaN);
    updateChain(obj, 5, 'foo', 9);
    assertEq(runChain_5_5(obj), 900);
}

//// Test chain of length 6 with late-property-addition at depth 0
function runChain_6_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_6_0() {
    var obj = createTower(6);
    assertEq(runChain_6_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_6_0(obj), 900);
}

//// Test chain of length 6 with late-property-addition at depth 1
function runChain_6_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_6_1() {
    var obj = createTower(6);
    assertEq(runChain_6_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_6_1(obj), 900);
}

//// Test chain of length 6 with late-property-addition at depth 2
function runChain_6_2(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_6_2() {
    var obj = createTower(6);
    assertEq(runChain_6_2(obj), NaN);
    updateChain(obj, 2, 'foo', 9);
    assertEq(runChain_6_2(obj), 900);
}

//// Test chain of length 6 with late-property-addition at depth 3
function runChain_6_3(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_6_3() {
    var obj = createTower(6);
    assertEq(runChain_6_3(obj), NaN);
    updateChain(obj, 3, 'foo', 9);
    assertEq(runChain_6_3(obj), 900);
}

//// Test chain of length 6 with late-property-addition at depth 4
function runChain_6_4(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_6_4() {
    var obj = createTower(6);
    assertEq(runChain_6_4(obj), NaN);
    updateChain(obj, 4, 'foo', 9);
    assertEq(runChain_6_4(obj), 900);
}

//// Test chain of length 6 with late-property-addition at depth 5
function runChain_6_5(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_6_5() {
    var obj = createTower(6);
    assertEq(runChain_6_5(obj), NaN);
    updateChain(obj, 5, 'foo', 9);
    assertEq(runChain_6_5(obj), 900);
}

//// Test chain of length 6 with late-property-addition at depth 6
function runChain_6_6(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_6_6() {
    var obj = createTower(6);
    assertEq(runChain_6_6(obj), NaN);
    updateChain(obj, 6, 'foo', 9);
    assertEq(runChain_6_6(obj), 900);
}

//// Test chain of length 7 with late-property-addition at depth 0
function runChain_7_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_7_0() {
    var obj = createTower(7);
    assertEq(runChain_7_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_7_0(obj), 900);
}

//// Test chain of length 7 with late-property-addition at depth 1
function runChain_7_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_7_1() {
    var obj = createTower(7);
    assertEq(runChain_7_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_7_1(obj), 900);
}

//// Test chain of length 7 with late-property-addition at depth 2
function runChain_7_2(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_7_2() {
    var obj = createTower(7);
    assertEq(runChain_7_2(obj), NaN);
    updateChain(obj, 2, 'foo', 9);
    assertEq(runChain_7_2(obj), 900);
}

//// Test chain of length 7 with late-property-addition at depth 3
function runChain_7_3(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_7_3() {
    var obj = createTower(7);
    assertEq(runChain_7_3(obj), NaN);
    updateChain(obj, 3, 'foo', 9);
    assertEq(runChain_7_3(obj), 900);
}

//// Test chain of length 7 with late-property-addition at depth 4
function runChain_7_4(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_7_4() {
    var obj = createTower(7);
    assertEq(runChain_7_4(obj), NaN);
    updateChain(obj, 4, 'foo', 9);
    assertEq(runChain_7_4(obj), 900);
}

//// Test chain of length 7 with late-property-addition at depth 5
function runChain_7_5(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_7_5() {
    var obj = createTower(7);
    assertEq(runChain_7_5(obj), NaN);
    updateChain(obj, 5, 'foo', 9);
    assertEq(runChain_7_5(obj), 900);
}

//// Test chain of length 7 with late-property-addition at depth 6
function runChain_7_6(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_7_6() {
    var obj = createTower(7);
    assertEq(runChain_7_6(obj), NaN);
    updateChain(obj, 6, 'foo', 9);
    assertEq(runChain_7_6(obj), 900);
}

//// Test chain of length 7 with late-property-addition at depth 7
function runChain_7_7(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_7_7() {
    var obj = createTower(7);
    assertEq(runChain_7_7(obj), NaN);
    updateChain(obj, 7, 'foo', 9);
    assertEq(runChain_7_7(obj), 900);
}

//// Test chain of length 8 with late-property-addition at depth 0
function runChain_8_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_8_0() {
    var obj = createTower(8);
    assertEq(runChain_8_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_8_0(obj), 900);
}

//// Test chain of length 8 with late-property-addition at depth 1
function runChain_8_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_8_1() {
    var obj = createTower(8);
    assertEq(runChain_8_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_8_1(obj), 900);
}

//// Test chain of length 8 with late-property-addition at depth 2
function runChain_8_2(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_8_2() {
    var obj = createTower(8);
    assertEq(runChain_8_2(obj), NaN);
    updateChain(obj, 2, 'foo', 9);
    assertEq(runChain_8_2(obj), 900);
}

//// Test chain of length 8 with late-property-addition at depth 3
function runChain_8_3(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_8_3() {
    var obj = createTower(8);
    assertEq(runChain_8_3(obj), NaN);
    updateChain(obj, 3, 'foo', 9);
    assertEq(runChain_8_3(obj), 900);
}

//// Test chain of length 8 with late-property-addition at depth 4
function runChain_8_4(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_8_4() {
    var obj = createTower(8);
    assertEq(runChain_8_4(obj), NaN);
    updateChain(obj, 4, 'foo', 9);
    assertEq(runChain_8_4(obj), 900);
}

//// Test chain of length 8 with late-property-addition at depth 5
function runChain_8_5(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_8_5() {
    var obj = createTower(8);
    assertEq(runChain_8_5(obj), NaN);
    updateChain(obj, 5, 'foo', 9);
    assertEq(runChain_8_5(obj), 900);
}

//// Test chain of length 8 with late-property-addition at depth 6
function runChain_8_6(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_8_6() {
    var obj = createTower(8);
    assertEq(runChain_8_6(obj), NaN);
    updateChain(obj, 6, 'foo', 9);
    assertEq(runChain_8_6(obj), 900);
}

//// Test chain of length 8 with late-property-addition at depth 7
function runChain_8_7(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_8_7() {
    var obj = createTower(8);
    assertEq(runChain_8_7(obj), NaN);
    updateChain(obj, 7, 'foo', 9);
    assertEq(runChain_8_7(obj), 900);
}

//// Test chain of length 8 with late-property-addition at depth 8
function runChain_8_8(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_8_8() {
    var obj = createTower(8);
    assertEq(runChain_8_8(obj), NaN);
    updateChain(obj, 8, 'foo', 9);
    assertEq(runChain_8_8(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 0
function runChain_9_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_0() {
    var obj = createTower(9);
    assertEq(runChain_9_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_9_0(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 1
function runChain_9_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_1() {
    var obj = createTower(9);
    assertEq(runChain_9_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_9_1(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 2
function runChain_9_2(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_2() {
    var obj = createTower(9);
    assertEq(runChain_9_2(obj), NaN);
    updateChain(obj, 2, 'foo', 9);
    assertEq(runChain_9_2(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 3
function runChain_9_3(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_3() {
    var obj = createTower(9);
    assertEq(runChain_9_3(obj), NaN);
    updateChain(obj, 3, 'foo', 9);
    assertEq(runChain_9_3(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 4
function runChain_9_4(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_4() {
    var obj = createTower(9);
    assertEq(runChain_9_4(obj), NaN);
    updateChain(obj, 4, 'foo', 9);
    assertEq(runChain_9_4(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 5
function runChain_9_5(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_5() {
    var obj = createTower(9);
    assertEq(runChain_9_5(obj), NaN);
    updateChain(obj, 5, 'foo', 9);
    assertEq(runChain_9_5(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 6
function runChain_9_6(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_6() {
    var obj = createTower(9);
    assertEq(runChain_9_6(obj), NaN);
    updateChain(obj, 6, 'foo', 9);
    assertEq(runChain_9_6(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 7
function runChain_9_7(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_7() {
    var obj = createTower(9);
    assertEq(runChain_9_7(obj), NaN);
    updateChain(obj, 7, 'foo', 9);
    assertEq(runChain_9_7(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 8
function runChain_9_8(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_8() {
    var obj = createTower(9);
    assertEq(runChain_9_8(obj), NaN);
    updateChain(obj, 8, 'foo', 9);
    assertEq(runChain_9_8(obj), 900);
}

//// Test chain of length 9 with late-property-addition at depth 9
function runChain_9_9(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_9_9() {
    var obj = createTower(9);
    assertEq(runChain_9_9(obj), NaN);
    updateChain(obj, 9, 'foo', 9);
    assertEq(runChain_9_9(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 0
function runChain_10_0(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_0() {
    var obj = createTower(10);
    assertEq(runChain_10_0(obj), NaN);
    updateChain(obj, 0, 'foo', 9);
    assertEq(runChain_10_0(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 1
function runChain_10_1(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_1() {
    var obj = createTower(10);
    assertEq(runChain_10_1(obj), NaN);
    updateChain(obj, 1, 'foo', 9);
    assertEq(runChain_10_1(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 2
function runChain_10_2(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_2() {
    var obj = createTower(10);
    assertEq(runChain_10_2(obj), NaN);
    updateChain(obj, 2, 'foo', 9);
    assertEq(runChain_10_2(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 3
function runChain_10_3(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_3() {
    var obj = createTower(10);
    assertEq(runChain_10_3(obj), NaN);
    updateChain(obj, 3, 'foo', 9);
    assertEq(runChain_10_3(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 4
function runChain_10_4(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_4() {
    var obj = createTower(10);
    assertEq(runChain_10_4(obj), NaN);
    updateChain(obj, 4, 'foo', 9);
    assertEq(runChain_10_4(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 5
function runChain_10_5(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_5() {
    var obj = createTower(10);
    assertEq(runChain_10_5(obj), NaN);
    updateChain(obj, 5, 'foo', 9);
    assertEq(runChain_10_5(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 6
function runChain_10_6(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_6() {
    var obj = createTower(10);
    assertEq(runChain_10_6(obj), NaN);
    updateChain(obj, 6, 'foo', 9);
    assertEq(runChain_10_6(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 7
function runChain_10_7(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_7() {
    var obj = createTower(10);
    assertEq(runChain_10_7(obj), NaN);
    updateChain(obj, 7, 'foo', 9);
    assertEq(runChain_10_7(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 8
function runChain_10_8(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_8() {
    var obj = createTower(10);
    assertEq(runChain_10_8(obj), NaN);
    updateChain(obj, 8, 'foo', 9);
    assertEq(runChain_10_8(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 9
function runChain_10_9(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_9() {
    var obj = createTower(10);
    assertEq(runChain_10_9(obj), NaN);
    updateChain(obj, 9, 'foo', 9);
    assertEq(runChain_10_9(obj), 900);
}

//// Test chain of length 10 with late-property-addition at depth 10
function runChain_10_10(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_10_10() {
    var obj = createTower(10);
    assertEq(runChain_10_10(obj), NaN);
    updateChain(obj, 10, 'foo', 9);
    assertEq(runChain_10_10(obj), 900);
}


/////////////////////////////////////////
// RUNNER                              //
/////////////////////////////////////////

testChain_0_0();
testChain_1_0();
testChain_1_1();
testChain_2_0();
testChain_2_1();
testChain_2_2();
testChain_3_0();
testChain_3_1();
testChain_3_2();
testChain_3_3();
testChain_4_0();
testChain_4_1();
testChain_4_2();
testChain_4_3();
testChain_4_4();
testChain_5_0();
testChain_5_1();
testChain_5_2();
testChain_5_3();
testChain_5_4();
testChain_5_5();
testChain_6_0();
testChain_6_1();
testChain_6_2();
testChain_6_3();
testChain_6_4();
testChain_6_5();
testChain_6_6();
testChain_7_0();
testChain_7_1();
testChain_7_2();
testChain_7_3();
testChain_7_4();
testChain_7_5();
testChain_7_6();
testChain_7_7();
testChain_8_0();
testChain_8_1();
testChain_8_2();
testChain_8_3();
testChain_8_4();
testChain_8_5();
testChain_8_6();
testChain_8_7();
testChain_8_8();
testChain_9_0();
testChain_9_1();
testChain_9_2();
testChain_9_3();
testChain_9_4();
testChain_9_5();
testChain_9_6();
testChain_9_7();
testChain_9_8();
testChain_9_9();
testChain_10_0();
testChain_10_1();
testChain_10_2();
testChain_10_3();
testChain_10_4();
testChain_10_5();
testChain_10_6();
testChain_10_7();
testChain_10_8();
testChain_10_9();
testChain_10_10();
