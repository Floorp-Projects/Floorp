// |jit-test| error:InternalError; skip-if: !Error.prototype.toSource

// Binary: cache/js-dbg-64-0906d9490eaf-linux
// Flags:
//
var e = Error('');
e.fileName = e;
e.toSource();
--> Crash with too much recursion in exn_toSource
