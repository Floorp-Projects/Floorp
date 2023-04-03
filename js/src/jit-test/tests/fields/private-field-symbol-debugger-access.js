// Make a new global to debug
const global = newGlobal({ newCompartment: true });

// Create an object in that global with a private field.
global.eval("\nclass MyClass {\n #privateProperty1\n }\nobj = new MyClass();");

// Debug said global.
const debug = Debugger();
const globalDebugObject = debug.addDebuggee(global);

// Leak the private name symbol backing the private field.
var otherGlobalObj = globalDebugObject.getOwnPropertyDescriptor("obj").value
var privateSymbol = otherGlobalObj.getOwnPrivateProperties()[0]

// Create a different proxy.
var p = new Proxy({}, {});

// Try to look up the leaked private symbol on the new proxy.
// This crashes, as it violates the assumption baked into the proxy code
// that all accesses are scripted, and thus creation and symbol management
// invariants are correctly observed.
fail = false;
try {
    p[privateSymbol] = 1;
    fail = true;
} catch (e) {
    assertEq(e instanceof TypeError, true);
}
assertEq(fail, false);

try {
    p[privateSymbol];
    fail = true;
} catch (e) {
    assertEq(e instanceof TypeError, true);
}
assertEq(fail, false);
