function f(b) {
	while (b < 30) {
		var y = b - 3;
		b++;
	}
	print(b);
}

for (var i = 0; i < 60; i++)
	f(i);
