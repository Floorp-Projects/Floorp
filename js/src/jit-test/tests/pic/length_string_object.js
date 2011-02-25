//length, string, object

var expected = "3,6,4,3,6,4,3,6,4,3,6,4,";
var actual = '';

function f() {
    var ss = [new String("abc"), new String("foobar"), new String("quux")];

    for (var i = 0; i < 12; ++i) {
        actual += ss[i%3].length + ',';
    }
}

f();

assertEq(actual, expected);


function g(s) {
    return new String(s).length;
}

assertEq(g("x"), 1); // Warm-up
assertEq(g("x"), 1); // Create IC
assertEq(g("x"), 1); // Test IC

function h(s) {
    var x = new String(s);
    for (var i = 0; i < 100; i++)
        x[i] = i;
    return x.length;
}

assertEq(h("x"), 1);
assertEq(h("x"), 1);
assertEq(h("x"), 1);
