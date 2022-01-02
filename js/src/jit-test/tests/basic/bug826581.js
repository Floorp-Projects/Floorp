// Don't crash.

try {
    x = "          ()    ";
    for (var y = 0; y < 19; y++) {
        x += x;
    }
} catch (e) {}

try {
	"".replace(x, "", "gy");
} catch (e) { }
