function gen() {
    try {
	yield 3;
    } finally {
	quit();
    }
}
try {
    for (var i in gen())
	foo();
} catch (e) {}
