function dummy() { with ({}) {}}

function foo() {
    var a = 0;
    var b = 1;
    var c = 2;
    var d = 3;

    // This call will run before we enter jitcode and won't have IC
    // data, so branch pruning will remove the path from the entry
    // block to the OSR preheader.
    dummy();

    // We will OSR in this loop. Because there's no path from the
    // function entry to the loop, the only information we have
    // about a, b, c, and d is that they come from the OSR block.
    for (var i = 0; i < 1000; i++) {

        // Create a bunch of phis that only depend on OsrValues.
        // These phis will be specialized to MIRType::Value.
        a = i % 2 ? b : c;
        b = i % 3 ? c : d;
        c = i % 4 ? d : a;
        d = i % 5 ? a : b;

        // This phi will be optimistically specialized to
        // MIRType::String and trigger a bailout.
        dummy(i % 6 ? d : "");
    }
    return a;
}
foo();
