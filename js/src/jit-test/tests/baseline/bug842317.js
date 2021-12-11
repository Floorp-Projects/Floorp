gczeal(2);
function A(a) { this.a = a; }
function B(b) { this.b = b; }
function C(c) { this.c = c; }
function makeArray(n) {
    var classes = [A, B, C];
    var arr = [];
    for (var i = 0; i < n; i++) {
        arr.push(new classes[i % 3](i % 3));
    }
}
var arr = makeArray(30000);
