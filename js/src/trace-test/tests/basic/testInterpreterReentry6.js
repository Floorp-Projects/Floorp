function testInterpreterReentry6() {
    var obj = {a:1, b:1, c:1, d:1, set e(x) { this._e = x; }};
    for (var p in obj)
        obj[p] = "grue";
    return obj._e;
}
assertEq(testInterpreterReentry6(), "grue");
