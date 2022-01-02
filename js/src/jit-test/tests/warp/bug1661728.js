with ({}) {} // Don't inline anything into the top-level script.

function foo() {}

function inline_foo_into_bar() {
    with ({}) {} // Don't inline anything into this function.
    for (var i = 0; i < 10; i++) {
        bar(2);
    }

}

function bar(x) {
    switch (x) {
    case 1:
        inline_foo_into_bar();

        // Trigger a compacting gc to discard foo's jitscript.
        // Do it while bar is on the stack to keep bar's jitscript alive.
        gc(foo, 'shrinking');
        break;
    case 2:
        foo();
        break;
    case 3:
        break;
    }
}

// Warm up foo and bar.
for (var i = 0; i < 10; i++) {
    foo();
    bar(3);
}

// Inline and discard foo's jitscript.
bar(1);

// Warp-compile bar
for (var i = 0; i < 50; i++) {
    foo(); // ensure that foo has a new jitscript
    bar(3);
}
