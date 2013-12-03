// |jit-test| error:is not a function
var arr = [];

var C = function () {};
C.prototype.dump = function () {};
arr[0] = new C;

C = function () {};
C.prototype.dump = this;
arr[1] = new C;

function f() {
    for (var i = 0; i < arr.length; i++)
        arr[i].dump();
}

try {
    f();
} catch (exc) {
    assertEq(exc.message.contains("is not a function"), true);
}
f();
