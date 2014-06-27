
// This code generates the test cases jit-test/tests/baseline/no-such-property-getprop.js
//
// In particular, it generates the testChain_<N>_<I>() and runChain_<N>_<I>() functions
// at the tail of the file.

var TEST_CASE_FUNCS =  [];
function runChain_NNNN_DDDD(obj) {
    var sum = 0;
    for (var i = 0; i < 100; i++)
        sum += obj.foo;
    return sum;
}
function testChain_NNNN_DDDD() {
    var obj = createTower(NNNN);
    assertEq(runChain_NNNN_DDDD(obj), NaN);
    updateChain(obj, DDDD, 'foo', 9);
    assertEq(runChain_NNNN_DDDD(obj), 900);
}
function generateTestCase(n, d) {
    var runFuncName = "runChain_" + n + "_" + d;
    var testFuncName = "testChain_" + n + "_" + d;
    TEST_CASE_FUNCS.push(testFuncName);

    print("//// Test chain of length " + n + " with late-property-addition at depth " + d);
    print(uneval(runChain_NNNN_DDDD).replace(/NNNN/g, ''+n).replace(/DDDD/g, ''+d));
    print(uneval(testChain_NNNN_DDDD).replace(/NNNN/g, ''+n).replace(/DDDD/g, ''+d));
    print("");
}

// Helper function to create an object with a proto-chain of length N.
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

print("/////////////////////////////////////////");
print("// This is a generated file!");
print("// See jit-tests/etc/generate-nosuchproperty-tests.js for the code");
print("// that generated this code!");
print("/////////////////////////////////////////");
print("");
print("/////////////////////////////////////////");
print("// PRELUDE                             //");
print("/////////////////////////////////////////");
print("");
print(createTower);
print(updateChain);
print("");
print("/////////////////////////////////////////");
print("// TEST CASES                          //");
print("/////////////////////////////////////////");
print("");
for (var n = 0; n <= 10; n++) {
    for (var d = 0; d <= n; d++) {
        generateTestCase(n, d);
    }
}

print("");
print("/////////////////////////////////////////");
print("// RUNNER                              //");
print("/////////////////////////////////////////");
print("");
for (var i = 0; i < TEST_CASE_FUNCS.length; i++)
    print(TEST_CASE_FUNCS[i] + "();");
