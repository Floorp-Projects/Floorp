// |jit-test| error: 42
load(libdir + "immutable-prototype.js");

function f(y) {}
for each(let e in newGlobal()) {
    if (e.name === "quit" || e.name == "readline" || e.name == "terminate" ||
	e.name == "nestedShell")
	continue;
        print(e.name);
    try {
        e();
    } catch (r) {}
}
(function() {
    arguments;
    if (globalPrototypeChainIsMutable())
        Object.prototype.__proto__ = newGlobal()
    function f(y) {
        y()
    }
    var arr = [];
    arr.__proto__ = newGlobal();
    for each (b in arr) {
        if (b.name === "quit" || b.name == "readline" || b.name == "terminate" ||
            b.name == "nestedShell")
            continue;
       try {
           f(b)
       } catch (e) {}
   }
})();

throw 42;
