x = {};
y = x;
x.toString = function() {
    Int8Array(ArrayBuffer)[0] = Float32Array(ArrayBuffer)[0];
}
print(x << y);
