// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-b6aa44d8f11f-linux
// Flags:
//

Array.prototype.iterator = (function() { { while(0) function Uint8ClampedArray() {  } } });
var s = new Set(["testing", "testing", 123]);
assertEq(s.size(), 2);
