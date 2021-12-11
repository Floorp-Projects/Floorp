// ECMA262 9.4.5.2 [[HasProperty]]
function check_in(x, a) {
    return (x in a);
}

function check_has_own(x, a) {
    return a.hasOwnProperty(x);
}

//make sure baseline gets compiled
function warmup(a) {
    for (var i = 0; i < 1001; i++) {
        check_in(i, a);
        check_has_own(i, a);
    }
}

function check_assertions(a) {
    assertEq(check_in(1, a),      true);
    assertEq(check_in("-0",a),    false); // -0 access
    assertEq(check_in(-10,a),     false); // Negative access
    assertEq(check_in(1012,a),    false); // OOB access


    assertEq(check_has_own(1, a),      true);
    assertEq(check_has_own("-0",a),    false); // -0 access
    assertEq(check_has_own(-10,a),     false); // Negative access
    assertEq(check_has_own(1012,a),    false); // OOB access
}

function test_with_no_protochain(a) {
    var a = new Int32Array(1000).fill(1);
    warmup(a);
    check_assertions(a);
}

// Attempting to validate against this comment:
// https://bugzilla.mozilla.org/show_bug.cgi?id=1419372#c3
//
// "Out of bounds "in" or "hasOwnProperty" should always
// return false, and not consider the prototype chain at all"
function test_with_protochain(a) {
    var a = new Int32Array(1000).fill(1);
    warmup(a);
    // try to force the behaviour of 9.4.5.2
    Object.prototype["-0"] = "value";
    Object.prototype[-1]   = "value";
    Object.prototype[-10]  = "value";
    Object.prototype[1012] = "value";
    check_assertions(a);
}

test_with_no_protochain();
test_with_protochain();
