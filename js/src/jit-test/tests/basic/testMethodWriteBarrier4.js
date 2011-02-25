var z = 0;
function f() {
    this.b = function() {};
    this.b = undefined;
    if (z++ > HOTLOOP)
        this.b();
}

try {
    for (var i = 0; i < HOTLOOP + 2; i++)
        new f();
} catch (exc) {}
