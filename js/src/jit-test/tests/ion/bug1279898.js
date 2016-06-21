
function f() {
    var i32 = new Int32Array(1);
    var f32 = new Float32Array(i32.buffer);
    for (var i = 0; i < 3; i++) {
        var a0 = +1;
        var a3 = +4;

        i32[0] = a0;
        var b0 = f32[0];

        i32[0] = a3;
        var b3 = f32[0];

        assertEq(b0 != b3, true);
    }
}

f();
