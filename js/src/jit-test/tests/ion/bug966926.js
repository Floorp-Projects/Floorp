var f32 = new Float32Array(32);
function f(n) {
    var x;
    if (n > 10000) {
        x = (0);
    } else {
        x = f32[0];
    }
    g('' + (x));
}
function g(y){}
f(0);
