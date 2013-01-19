// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-6c8becdd1574-linux
// Flags:
//
(function () {
    [] = x = /x/;
    x.toString = Function.prototype.bind;
    print(x)
})()
