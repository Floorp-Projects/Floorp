function f() {
    var o = {x: 0};
    for (var i = 0; i < 20; i++) {
        if ((i % 4) === 0)
            Object.setPrototypeOf(o, null);
        else
            Object.setPrototypeOf(o, Object.prototype);
        for (var x in o) {}
    }
}
f();
