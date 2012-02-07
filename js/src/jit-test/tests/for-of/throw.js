// Control can exit a for-of loop via throw.

function f() {
    for (var a of [1, 2, 3]) {
        for (var b of [1, 2, 3]) {
            for (var c of [1, 2, 3]) {
                if (a !== b && b !== c && c !== a)
                    throw [a, b, c];
            }
        }
    }
}

var x = null;
try {
    f();
} catch (exc) {
    x = exc.join("");
}
assertEq(x, "123");
