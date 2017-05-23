// --ion-eager --ion-offthread-compile=off

var threw = 0;

function f(t) {
    for (var i = 0; i < 2; i++) {
        try {
            var x = 1;
            Array(1);
            x = 2;
            x = t + t; // May throw if too large
        } catch (e) {
            assertEq(x, 2);
            threw++;
        }
    }
}

var x = '$';
for (var i = 0; i < 32; ++i) {
    with({}){}
    f(x);
    try { x = x + x; } catch (e) { }
}

// Sanity check that throws happen
assertEq(threw > 0, true);
