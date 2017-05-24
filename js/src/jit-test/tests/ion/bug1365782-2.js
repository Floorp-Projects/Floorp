// --ion-eager --ion-offthread-compile=off

function f(t) {
    var would_throw = false;
    try { t + t; } catch (e) { would_throw = true; }

    var s = 0;
    for (var i = 0; i < 2; i++) {
        if (!would_throw) {
            var x = 1;
            Array(1);
            x = 2;
            x = t + t; // May throw if too large
            s += x.length;
        }
    }
    return s;
}

var x = '$';
for (var i = 0; i < 32; ++i) {
    with({}){}
    f(x);
    try { x = x + x; } catch (e) { }
}
