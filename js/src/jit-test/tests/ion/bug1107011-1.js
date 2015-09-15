var f32 = new Float32Array(32);
function f(n) {
    var x;
    if (n > 10000) {
        x = 4.5;
    } else {
        x = f32[0];
    }
    f32[0] = (function() {
        for(var f=0;f<4;++f) {
            x=1;
        }
    })() < x;
}
for (var n = 0; n < 100; n++)
    f(n);

