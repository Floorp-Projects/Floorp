var global = 0;
Object.defineProperty(Array.prototype, "2", {get: function() { return global++; }});

function f() {
    var a = [];
    var b = [0, 1, , 3];
    for (var i=0; i<100; i++)
        var c = a.concat(b);
    assertEq(c[2], 99);
}
f();
