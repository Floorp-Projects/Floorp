// Different representations of the same number or string are treated as the same Map key.

var m = new Map;
var test = function test(a, b) {
    m.set(a, 'secret');
    assertEq(m.get(b), 'secret');
    m.set(b, 'password');
    assertEq(m.get(a), 'password');

    assertEq(a, b);
};

// Float and integer representations of the same number are the same key.
test(9, Math.sqrt(81));

// Ordinary strings and ropes are the same key.
var a = Array(1001).join('x');
var b = Array(501).join('x') + Array(501).join('x');
test(a, b);

// Two structurally different ropes with the same characters are the same key.
a = "";
b = "";
for (var i = 0; i < 10; i++) {
    a = Array(501).join('x') + a;
    b = b + Array(501).join('x');
}
test(a, b);
