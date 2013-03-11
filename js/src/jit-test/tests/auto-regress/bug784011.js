// Binary: cache/js-dbg-64-c676b554c7bb-linux
// Flags:
//

var p2 = new ParallelArray([2,2], function(i,j) { return i+j; });
p2.get({ 0: 1, 1: 0, testGet: 2 })
