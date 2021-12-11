// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-0428dbdf3d58-linux
// Flags:
//
o = (new Uint32Array).buffer
o.__proto__ = o
o.__proto__ = o
