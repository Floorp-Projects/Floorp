function g() { }

g();


function f(a, g) {
    var x;
    if (a) {
        x = 12;
        print(a);
	x = a + 19;
    } else {
        x = 20 + a;
        g(a);
        x += a;
    }
    return a + x + 12;
}
assertEq(f(0, g), 32);
