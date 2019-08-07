var log = "";
function g() {
    var x = [];
    for (var k = 0; k < 2; ++k) {
        x.push(k);
    }
    log += x;
}
for (var i = 0; i < 1; i++) {
    f = function() {};
}
g();
Array.prototype.push = f;
g();
f.__proto__ = [];
g();
assertEq(log, "0,1");
