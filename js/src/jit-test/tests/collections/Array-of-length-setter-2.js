// Array.of does a strict assignment to the new object's .length.
// The assignment is strict even if the code we're calling from is not strict.

load(libdir + "asserts.js");

function Empty() {}
Empty.of = Array.of;
Object.defineProperty(Empty.prototype, "length", {get: () => 0});

var nothing = new Empty;
nothing.length = 2;  // no exception; this is not a strict mode assignment

assertThrowsInstanceOf(() => Empty.of(), TypeError);
