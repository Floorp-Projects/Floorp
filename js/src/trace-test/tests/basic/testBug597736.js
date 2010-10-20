function leak_test() {
    // Create a reference loop function->script->traceFragment->object->function
    // that GC must be able to break. To embedd object into the fragment the
    // code use prototype chain of depth 2 which caches obj.__proto__.__proto__
    // into the fragment.

    // To make sure that we have no references to the function f after this
    // function returns due via the conservative scan of the native stack we
    // loop here multiple times overwriting the stack and registers with new garabge.
    for (var j = 0; j != 8; ++j) {
	var f = Function("a", "var s = 0; for (var i = 0; i != 100; ++i) s += a.b; return s;");
	var c = {b: 1, f: f, leakDetection: makeFinalizeObserver()};
	f({ __proto__: { __proto__: c}});
	f = c = a = null;
	gc();
    }
}

function test()
{
    if (typeof finalizeCount != "function")
	return;

    var base = finalizeCount();
    leak_test();
    gc();
    gc();
    var n = finalizeCount();
    assertEq(base + 4 < finalizeCount(), true, "Some finalizations must happen");
}

test();
