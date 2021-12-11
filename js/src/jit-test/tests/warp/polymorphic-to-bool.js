// |jit-test| --fast-warmup; --no-threads

function runTest(src, seen, last) {
    with ({}) {}

    // Make a fresh script.
    var foo = eval(src);

    // Compile it with polymorphic types.
    for (var j = 0; j < 100; j++) {
	foo(seen[j % seen.length]);
    }

    // Now test the type sorted last.
    assertEq(foo(last), false);
}

function runTests(src) {
    // Each of these |seen| sets will cause the |last| type to be
    // the last type tested in testValueTruthyKernel.
    runTest(src, [1n,   Symbol("a"), 1.5,  "",   {}   ],  1);
    runTest(src, [1n,   Symbol("a"), 1.5,  "",   true ],  {});
    runTest(src, [1n,   Symbol("a"), 1.5,  true, {}   ],  "a");
    runTest(src, [1n,   Symbol("a"), true, "",   {}   ],  1.5);
    runTest(src, [1n,   true,        1.5,  "",   {}   ],  Symbol("a"));
    runTest(src, [true, Symbol("a"), 1.5,  "",   {}   ],  1n);
}

// JumpIfFalse
runTests("(x) => { if (x) { return false; }}");

// And
runTests("(x) => x && false");

// Or
runTests("(x) => !(x || true)");

// Not
runTests("(x) => !x");
