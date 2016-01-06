function f() {
    for (var e = 1; e < 3000; e++) {
	(function(arguments) {
            eval("var y");
	})();
    }
}
f();
