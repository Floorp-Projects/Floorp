// --ion-eager --ion-offthread-compile=off

function f(t) {
    for (var i = 0; i < 2; i++) {
        try {
            var x = 1;
            new String();
            x = 2;
            `${t}`;         // Throws a TypeError
        } catch (e) {
            assertEq(x, 2);
        }
    }
}

f(Symbol());
