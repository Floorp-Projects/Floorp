function testAtomize() {
    x = {};
    for (var i = 0; i < 65536; ++i)
	x[String.fromCharCode(i)] = 1;
    var z = 0;
    for (var p in x)
	++z;
    return z;
}
assertEq(testAtomize(), 65536)
