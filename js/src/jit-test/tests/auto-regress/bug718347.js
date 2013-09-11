// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-49afabda6701-linux
// Flags: -m -n -a
//

this.__proto__ = newGlobal();
eval("(toLocaleString)();");
