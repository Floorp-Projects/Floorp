
// Test setting return value;

function bail() { bailout(); }
function bail2() { bailout(); return 2; }

// Test 1: Test setting/getting return value in ionmonkey
function test() {
    return evalcx("1;");
}
assertEq(test(), 1)

// Test 3: Test ion -> baseline
function test2() {
    return evaluate("1; bail2();");
}
assertEq(test2(), 2)

// Test 3: Test ion -> baseline
function test3() {
    return evaluate("1; bail2(); 3");
}
assertEq(test3(), 3)

// Test4: Test baseline -> ion entering (very fragile, since iterations need to be precise, before it gets tested)
function test4() {
    return evaluate("1; for(var i=0; i<1097; i++) { 3; };");
}
assertEq(test4(), 3)
