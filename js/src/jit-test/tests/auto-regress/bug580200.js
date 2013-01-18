// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-c271aa43c7ab-linux
// Flags:
//
(function () {
    x = Proxy.create((function () {
        return {
            enumerateOwn: function () Object.getOwnPropertyDescriptor
        }
    })(), [])
})()(uneval(this))
