
function f(x) {
    for (var i = 0; i < 40; ++i) {
	var stack = getBacktrace({args: true});
	(function() { g = x;});
    }
}
f(1);
