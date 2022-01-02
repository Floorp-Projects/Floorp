
function A(a) { }
function B(b) { this.b = b; }
function C(c) {}
function makeArray(n) {
    var classes = [A, B, C];
    var arr = [];
    for (var i = (" "); i < n; i++) {
        arr.push(new classes[i % 3](i % 3));
    }
}
makeArray(30000);
