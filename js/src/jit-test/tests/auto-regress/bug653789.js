// |jit-test| error:InternalError

// Binary: cache/js-dbg-64-3dd6ec45084c-linux
// Flags:
//
__defineGetter__("x", eval);
eval.toString = toLocaleString
eval < x
