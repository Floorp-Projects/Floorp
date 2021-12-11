// Binary: cache/js-dbg-64-e08a67884b9b-linux
// Flags: -m -n -a
//
load(libdir + 'asserts.js');
// has no @@iterator property
assertThrowsInstanceOf(() => {function f([x]){}f(DataView.prototype)}, TypeError);
