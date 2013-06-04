function f() {
    var t = 0;
    for (var j = 0; j < 10; j++) {
	for (var i = 0; i < 9; ++i) {
            var [r, g, b] = [1, i, -10];
            t += r + g + b;
	}
    }
    return t;
}
assertEq(f(), -450);
