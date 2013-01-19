// |jit-test| error:Error

// Binary: cache/js-dbg-64-c676b554c7bb-linux
// Flags:
//

var p = new ParallelArray([1,25e8 ,3,4]);
var pp = p.partition(.34 );
