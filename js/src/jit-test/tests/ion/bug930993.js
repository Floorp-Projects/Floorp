x = {};
y = x;
x.toString = function() {
    new Int8Array(ArrayBuffer)[0] = new Float32Array(ArrayBuffer)[0];
}
print(x << y);
