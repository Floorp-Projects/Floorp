function* gen() {
    try {
	yield 3;
    } finally {
	quit();
    }
}
try {
    for (var i of gen())
	foo();
} catch (e) {}
