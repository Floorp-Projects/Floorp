var a = [true, false];
for (var i = 0; i < 1e4; i++) {
	var str = "x: " + a[i & 1];
	if (i & 1) {
		assertEq(str, "x: false");
	} else {
		assertEq(str, "x: true");
	}
}
