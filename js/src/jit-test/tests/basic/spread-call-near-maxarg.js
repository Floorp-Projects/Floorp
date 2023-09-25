// FIXME: ASAN and debug builds run this too slowly for now.
if (!getBuildConfiguration("debug") && !getBuildConfiguration("asan")) {
    let longArray = [];
    longArray.length = getMaxArgs() - 1;
    let shortArray = [];
    let a;

    let f = function() {
    };

    // Call_Scripted
    //   Optimized stub is used after some calls.
    a = shortArray;
    for (let i = 0; i < 4; i++) {
        if (i == 3) {
            a = longArray;
        }
        try {
            f(...a);
        } catch (e) {
            assertEq(e.message, "too much recursion");
        }
    }

    // Call_Scripted (constructing)
    a = shortArray;
    for (let i = 0; i < 4; i++) {
        if (i == 3) {
            a = longArray;
        }
        try {
            new f(...a);
        } catch (e) {
            assertEq(e.message, "too much recursion");
        }
    }

    // Call_Native
    a = shortArray;
    for (let i = 0; i < 4; i++) {
        if (i == 3) {
            a = longArray;
        }
        try {
            Math.max(...a);
        } catch (e) {
            assertEq(e.message, "too much recursion");
        }
    }

    // Call_Native (constructing)
    a = shortArray;
    for (let i = 0; i < 4; i++) {
        if (i == 3) {
            a = longArray;
        }
        try {
            new Date(...a);
        } catch (e) {
            assertEq(e.message, "too much recursion");
        }
    }

    // No optimized stub for eval.
    a = longArray;
    try {
        eval(...a);
    } catch (e) {
        assertEq(e.message, "too much recursion");
    }
}
