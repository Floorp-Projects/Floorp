function f() {
    var MAX_HEIGHT = 512;
    var obj = {};
    for (var i = 0; i < MAX_HEIGHT; i++)
        obj['a' + i] = i;
    obj.m = function () { return 0; };
}
f();
f();
