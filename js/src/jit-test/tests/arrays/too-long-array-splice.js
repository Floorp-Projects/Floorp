// |jit-test| allow-oom
// array.splice should throw if array.length is too large.

var length = 4294967295;
var array = new Array(length);
array.splice(100);
