
var t = 0;
var y = [];
y.toString = (function() { t += 1 });
function test() {
    for (var i = 0; i < 14; i++) {
        String.prototype.sup.call(y);
    }
}
test();
assertEq(t, 14);
