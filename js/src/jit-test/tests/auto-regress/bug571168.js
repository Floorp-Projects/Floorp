// |jit-test| error:InternalError

// Binary: cache/js-dbg-64-b84d0be52070-linux
// Flags:
//
var x = new Proxy(Function, {});
if (x.__proto__ = x) {
    print(x);
}
