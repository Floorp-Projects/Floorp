var f32 = new Float32Array(10);
f32[0] = 5;
var i = 0;
do {
    f32[i + 1] = f32[i] - 1;
    i += 1;
} while (f32[i]);

