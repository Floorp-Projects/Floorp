// Try to GC some ion code that involves an IC on x64.

var o = {
	bar: function () { return 1; }
}

function f(o) {
	for (var i = 0; i < 100; i++)
	 o.bar();
}

f(o);

// Don't segfault.
gc();
