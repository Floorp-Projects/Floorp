var r = /^b/;

// Create a long enough input string to time out.
var s = "a";
try {
    while (true) {
	s += s;
    }
} catch {}

startgc(7,'shrinking');

// Time out during slow match.
timeout(1);
assertEq(s.match(r), null);
