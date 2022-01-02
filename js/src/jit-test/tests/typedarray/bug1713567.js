// |jit-test| --ion-gvn=off; --fast-warmup; --no-threads

var heap = new ArrayBuffer(4);
var view32 = new Uint32Array(heap);

function foo(i0) {
    var t1 = i0 + 1;
    var t2 = i0 >>> view32[0];
    var t3 = t1 - (t2 > 0);
    return t3;
}

with ({}) {}
view32[0] = 0x80000000;
for (var i = 0; i < 100; i++) {
    foo(0);
}
view32[0] = 0;
for (var i = 0; i < 100; i++) {
    foo(0x80000000);
}
