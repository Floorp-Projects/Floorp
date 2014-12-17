// Returning primitive from [[Enumerate]] handler
load(libdir + "asserts.js");

let handler = {
    enumerate: function() {
        return undefined;
    }
};

let proxy = new Proxy({}, handler);

assertThrowsInstanceOf(function() {
    for (let x in proxy)
        assertEq(true, false);
}, TypeError)
