for (var i = 0; i < 20; i++) {
    m = Math.min(0xffffffff, 0);
}
assertEq(m == 0, true);

var buffer = new ArrayBuffer(4);
var int32View = new Int32Array(buffer);
var uint32View = new Uint32Array(buffer);
int32View[0] = -1;
var m;
for (var i = 0; i < 20; i++) {
    m = Math.min(uint32View[0], 0); // uint32View[0] == 0xffffffff
}
assertEq(m == 0, true);

