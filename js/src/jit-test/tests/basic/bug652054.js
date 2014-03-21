var M4x4 = {};
M4x4.mul = function M4x4_mul(a, b, r) {
    a11 = a[0]
    a21 = a[1]
    a31 = a[2]
    a12 = a[4]
    a22 = a[5]
    a32 = a[6]
    a13 = a[8]
    a23 = a[9]
    a33 = a[10]
    a14 = a[12]
    a24 = a[13]
    a34 = a[14]
    b[3]
    b[4]
    b13 = b[8]
    b23 = b[9]
    b33 = b[10]
    b43 = b[11]
    r[8] = a11 * b13 + a12 * b23 + a13 * b33 + a14 * b43
    r[9] = a21 * b13 + a22 * b23 + a23 * b33 + a24 * b43
    r[10] = a31 * b13 + a32 * b23 + a33 * b33 + a34 * b43
    return r;
};
M4x4.scale3 = function M4x4_scale3(x, y, z, m) {
    m[0] *= x;
    m[3] *= x;
    m[4] *= y;
    m[11] *= z;
};
M4x4.makeLookAt = function M4x4_makeLookAt() {
    tm1 = new Float32Array(16);
    tm2 = new Float32Array(16);
    r = new Float32Array(16)
    return M4x4.mul(tm1, tm2, r);
};
var jellyfish = {};
jellyfish.order = [];
function jellyfishInstance() {}
jellyfishInstance.prototype.drawShadow = function () {
    pMatrix = M4x4.makeLookAt();
    M4x4.mul(M4x4.makeLookAt(), pMatrix, pMatrix);
    M4x4.scale3(6, 180, 0, pMatrix);
}
function drawScene() {
    jellyfish.order.push([0, 0])
    jellyfish[0] = new jellyfishInstance()
    for (var i = 0, j = 0; i < jellyfish.count, j < 30; ++j) {
        jellyfish.order[i][0]
        jellyfish[0].drawShadow();
    }
}
drawScene();

