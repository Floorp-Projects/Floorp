function thrower1(x) {
    throw x + 2;

    // Dead code, should be ignored.
    throw ++x;
    return x;
}
function test1() {
    // If we ever inline functions containing JSOP_THROW,
    // this shouldn't assert.
    function f(x) {
        thrower1(x + 1);
    }
    for (var i=0; i<11000; i++) {
        try {
            f(i);
            assertEq(0, 1);
        } catch(e) {
            assertEq(e, i + 3);
        }
    }
}
test1();

// Test throwing from an uncompilable (interpreted) function.
function getException(f) {
    try {
        f();
        assertEq(0, 1);
    } catch(e) {
        return e;
    }
    assertEq(0, 1);
}

function thrower2(x) {
    if (x > 90)
        throw x;
    with ({}) {}; // Abort compilation...(?)
}
function test2() {
    for (var i = 0; i < 100; i++) {
        thrower2(i);
    }
}
assertEq(getException(test2), 91);

// Throwing |this| from a constructor.
function thrower3(x) {
    this.x = x;
    if (x > 90)
        throw this;
}
function test3() {
    for (var i=0; i < 100; i++) {
        new thrower3(i);
    }
}
assertEq(getException(test3).x, 91);

// Throwing an exception in various loop blocks.
var count = 0;
function thrower4(x) {
    throw count++;
    count += 12345; // Shouldn't be executed.
}
function test4_1() {
    var i = 0;
    for (new thrower4(i); i < 100; i++) {
        count += 2000; // Shouldn't be executed.
    }
}
function test4_2() {
    for (var i = 0; thrower4(i); i++) {
	count += 3000; // Shouldn't be executed.
    }
}
function test4_3() {
    for (var i = 0; i < 100; thrower4(i)) {
	count += 5;
    }
}
function test4_4() {
    for (var i = 0; i < 10; i++) {
        if (i > 8)
            thrower4();
        count += i;
    }
}
for (var i = 0; i < 100; i++) {
    assertEq(getException(test4_1), count-1);
    assertEq(getException(test4_2), count-1);
    assertEq(getException(test4_3), count-1);
    assertEq(getException(test4_4), count-1);
}
assertEq(count, 4500);
