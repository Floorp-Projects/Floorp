function f(b) {
	if (b < 30) {
		var y = b + 1;
	} else {
		var z = b + 2;
	}
	print(b);
}

for (var i = 0; i < 60; i++)
	f(i);
