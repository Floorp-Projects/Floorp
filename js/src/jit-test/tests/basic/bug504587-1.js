// This test case failed a WIP patch. See https://bugzilla.mozilla.org/show_bug.cgi?id=504587#c68

function B() {}
B.prototype.x = 1;
var d = new B;

var names = ['z', 'z', 'z', 'z', 'z', 'z', 'z', 'x'];
for (var i = 0; i < names.length; i++) {
    x = d.x;  // guard on shapeOf(d)
    d[names[i]] = 2;  // unpredicted shape change
    y = d.x;  // guard here is elided
}
assertEq(y, 2);  // Assertion failed: got 1, expected 2
