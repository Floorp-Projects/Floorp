// |jit-test| --ion-offthread-compile=off; --ion-warmup-threshold=30

// We disable any off-main thread compilation, and set a definite trigger for
// Ion compilation, such that we can garantee that we would OSR into the inner
// loop before we reach the end of the loop.
gcPreserveCode();

function f (n) {
    while (!inIon()) {
        var inner = 0;
        let x = {};
        for (var i = 0; i < n; i++) {
            inner += inIon() == true ? 1 : 0;
            if (inner <= 1)
                bailout();
        }
        assertEq(inner != 1, true);
    }
}

// Iterate enough to ensure that we OSR in this inner loop.
f(300);
