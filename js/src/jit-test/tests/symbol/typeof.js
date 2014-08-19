function sym() {
    return typeof Symbol === "function" ? Symbol() : "symbol";
}

function typ() {
    return typeof Symbol === "function" ? "symbol" : "string";
}

var a = [0, 0, 0, 0, 0, sym(), sym()];
var b = [];
function f(i, v) {
    b[i] = typeof v;
}
for (var i = 0; i < a.length; i++)
    f(i, a[i]);
assertEq(b[b.length - 2], typ());
assertEq(b[b.length - 1], typ());

