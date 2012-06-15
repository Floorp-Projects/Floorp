function f(b) {
	while (b < 200000000) {
		b++;
	}
	print(b);
}

for (var i = 0; i < 60; i++) {
	f(i);
}
