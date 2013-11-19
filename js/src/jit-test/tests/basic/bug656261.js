function build_getter(i) {
    var x = [i];
    return function f() { return x; }
}

function test()
{
    var N = internalConst("INCREMENTAL_MARK_STACK_BASE_CAPACITY") + 2;
    var o = {};
    var descriptor = { enumerable: true};
    for (var i = 0; i != N; ++i) {
	descriptor.get = build_getter(i);
	Object.defineProperty(o, i, descriptor);
    }

    // At this point we have an object o with N getters. Each getter in turn
    // is a closure storing an array. During the GC we push to the object
    // marking stack all the getters found in an object after we mark it. As N
    // exceeds the size of the object marking stack, this requires to run the
    // dealyed scanning for some closures to mark the array objects stored in
    // them.
    //
    // We run the GC twice to make sure that the background finalization
    // finishes before we access the objects.
    gc();
    gc();
    for (var i = 0; i != N; ++i)
	assertEq(o[i][0], i);
}

test();
