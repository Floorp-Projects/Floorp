// |jit-test| allow-oom
// array.splice should throw if array.length is too large.

var length = 4294967295;
var array = new Array(length);

// Disable any fast paths by adding an indexed property on the prototype chain.
Array.prototype[0] = 0;

array.splice(100);
