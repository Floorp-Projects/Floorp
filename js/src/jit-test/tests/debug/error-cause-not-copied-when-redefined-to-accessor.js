// Test that the ErrorCopier doesn't copy the optional "cause" property when it
// has been redefined to an accessor property.

var g = newGlobal({newCompartment: true});

var obj = g.eval(`
new Proxy({}, {
  isExtensible() {
    // Create an error object with an initial cause.
    let error = new Error("message", {cause: "initial cause"});

    // Ensure the "cause" property is correctly installed.
    assertEq(error.cause, "initial cause");

    // Redefine the "cause" data property to an accessor property.
    Object.defineProperty(error, "cause", { get() {} });

    // Throw the error.
    throw error;
  }
});
`);

var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var objw = gw.makeDebuggeeValue(obj);

var err;
try {
  objw.isExtensible();
} catch (e) {
  err = e;
}

assertEq(err.cause, undefined);
