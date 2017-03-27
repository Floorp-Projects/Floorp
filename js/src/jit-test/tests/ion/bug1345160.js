function f() {
    var o = [1, 2, 3];
    o.constructor = Uint8Array;
    for (var i=0; i<10; i++)
        o.filter(x => true);
}
f();
