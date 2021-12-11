var arrayView = new Float32Array(new ArrayBuffer(40));

function foo() {
    var x = arrayView[0];
    if (!x) {
        x = arrayView[NaN];
    }
    return x;
}

with ({}) {}
for (var i = 0; i < 100; i++) {
    foo();
}
