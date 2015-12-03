if (!this.SharedArrayBuffer)
    quit(0);

function loadModule_uint16(stdlib, foreign, heap) {
    "use asm";
    var atomic_add = stdlib.Atomics.add;
    var atomic_sub = stdlib.Atomics.sub;
    var atomic_and = stdlib.Atomics.and;
    var atomic_or = stdlib.Atomics.or;
    var atomic_xor = stdlib.Atomics.xor;
    var i16a = new stdlib.Uint16Array(heap);
    function do_add_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_add(i16a, i>>1, 0x3333)|0;
    }
    function do_sub_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_sub(i16a, i>>1, 0x3333)|0;
    }
    function do_and_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_and(i16a, i>>1, 0x3333)|0;
    }
    function do_or_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_or(i16a, i>>1, 0x3333)|0;
    }
    function do_xor_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_xor(i16a, i>>1, 0x3333)|0;
    }
    return { add_i: do_add_i,
	     sub_i: do_sub_i,
	     and_i: do_and_i,
	     or_i: do_or_i,
	     xor_i: do_xor_i };
}

function test_uint16(heap) {
    var i16m = loadModule_uint16(this, {}, heap);
    var size = Uint16Array.BYTES_PER_ELEMENT;
    i16m.add_i(size*40)
    i16m.sub_i(size*40)
    i16m.and_i(size*40)
    i16m.or_i(size*40)
    i16m.xor_i(size*40)
}

var heap = new SharedArrayBuffer(65536);
test_uint16(heap);
