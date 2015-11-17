for (var i = 0; i < 90; ++i) {
    y = {x: 1};
}

Object.defineProperty(Object.prototype, "zz", {set: (v) => 1 });

function f() {
    for (var i=0; i<1500; i++) {
        y[0] = 0;
        if (i > 1400)
            y.zz = 3;
    }
}
f();
