// The teleporting optimization should work correctly
// when we modify an object's proto.

var A = {x: 1};
var B = Object.create(A);

var C = {};
C.__proto__ = B;

function f() {
    for (var i=0; i<25; i++) {
        assertEq(C.x, (i <= 20) ? 1 : 3);
        if (i === 20) {
            B.x = 3;
        }
    }
}
f();
