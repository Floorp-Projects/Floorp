function m(stdlib, ffi, heap) {
    "use asm";
    var HEAP32 = new stdlib.SharedInt32Array(heap);
    var add = stdlib.Atomics.add;
    var load = stdlib.Atomics.load;
    function add_sharedEv(i1) {
	i1 = i1 | 0;
	load(HEAP32, i1 >> 2);
	add(HEAP32, i1 >> 2, 1);
	load(HEAP32, i1 >> 2);
    }
    return {add_sharedEv:add_sharedEv};
}
var sab = new SharedArrayBuffer(65536);
var {add_sharedEv} = m(this, {}, sab);
add_sharedEv(sab.byteLength);

