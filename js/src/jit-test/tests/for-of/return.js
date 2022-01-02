// Control can exit a for-of loop via return.

function f() {
    for (var a of [1, 2, 3]) {
        for (var b of [1, 2, 3]) {
            for (var c of [1, 2, 3]) {
                if (a !== b && b !== c && c !== a)
                    return "" + a + b + c;
            }
        }
    }
}

assertEq(f(), "123");
