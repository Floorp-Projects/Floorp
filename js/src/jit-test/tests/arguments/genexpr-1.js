// No 'arguments' binding in genexprs at toplevel.

load(libdir + "asserts.js");

delete this.arguments;  // it is defined in the shell
var iter = (arguments for (x of [1]));
assertThrowsInstanceOf(() => iter.next(), ReferenceError);
