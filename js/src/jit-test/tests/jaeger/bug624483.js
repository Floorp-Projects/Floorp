var arr = new Uint8ClampedArray(16);
for (var i = 0; i < 16; i++) {
    arr[i] = "Infinity";
}
assertEq(arr[14], 255);
