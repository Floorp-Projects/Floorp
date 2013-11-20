// |jit-test| error: 42
function f(y) {}
for each(let e in newGlobal()) {
    if (e.name === "quit" || e.name == "readline" || e.name == "terminate" ||
	e.name == "nestedShell")
	continue;
    try {
        e();
    } catch (r) {}
}
(function() {
    arguments.__proto__.__proto__ = newGlobal()
    function f(y) {
        y()
    }
    for each(b in []) {
	if (b.name === "quit" || b.name == "readline" || b.name == "terminate" ||
	    b.name == "nestedShell")
	    continue;
        try {
            f(b)
        } catch (e) {}
    }
})();

throw 42;
