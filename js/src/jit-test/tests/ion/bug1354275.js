// --ion-eager --ion-offthread-compile=off

function f(t) {
    for (var i = 0; i < 2; i++) {
        try {
            var x = 1;
            Array(1);
            x = 2;
            Array(t);
        } catch (e) {
            assertEq(x, 2);
        }
    }
}

f(-1);
