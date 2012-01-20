Date.prototype.format1 = function() {
    return "" + this.getMonth() + this.getFullYear();
}
function f(d) {
    for (var i=0; i<60; i++) {
        assertEq(d.format1(), "91987");
    }
}
f(new Date("10/10/1987 1:11:11"));

var global = 0;
function f1() {
    return global++;
}
function g1() {
    return f1() + f1();
}
var result = 0;
for (var i=0; i<100; i++)
    result += g1();
assertEq(result, 19900);

function g2() {
    var a = [];
    var b = [];
    a.push(1);
    return a.length + b.length;
}
for (var i=0; i<100; i++)
    assertEq(g2(), 1);
