// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-2ee92d697741-linux
// Flags: -m
//
{
    gczeal(2)
}
(function () {
    ''.w()
})()
