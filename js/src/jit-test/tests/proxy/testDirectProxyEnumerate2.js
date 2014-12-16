// Throwing [[Enumerate]] handler
load(libdir + "asserts.js");

let handler = new Proxy({}, {
    get: function(target, name) {
        assertEq(name, "enumerate");
        throw new SyntaxError();
    }
});

let proxy = new Proxy({}, handler);

assertThrowsInstanceOf(function() {
    for (let x in proxy)
        assertEq(true, false);
}, SyntaxError)
