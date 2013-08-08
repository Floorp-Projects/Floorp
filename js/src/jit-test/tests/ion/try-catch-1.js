function F() {
    try {
	var T = {};
        throw 12;
    } catch (e) {
	// Don't throw.
        T.x = 5;
    }
}
F();
F();
F();
