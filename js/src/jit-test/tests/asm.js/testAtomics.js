// |jit-test| test-also-noasmjs
if (!this.SharedArrayBuffer || !this.SharedInt32Array || !this.Atomics)
    quit();

// The code duplication below is very far from elegant but provides
// flexibility that comes in handy several places.

function loadModule_int32(stdlib, foreign, heap) {
    "use asm";

    var atomic_fence = stdlib.Atomics.fence;
    var atomic_load = stdlib.Atomics.load;
    var atomic_store = stdlib.Atomics.store;
    var atomic_cmpxchg = stdlib.Atomics.compareExchange;
    var atomic_add = stdlib.Atomics.add;
    var atomic_sub = stdlib.Atomics.sub;
    var atomic_and = stdlib.Atomics.and;
    var atomic_or = stdlib.Atomics.or;
    var atomic_xor = stdlib.Atomics.xor;

    var i32a = new stdlib.SharedInt32Array(heap);

    function do_fence() {
	atomic_fence();
    }

    // Load element 0
    function do_load() {
	var v = 0;
	v = atomic_load(i32a, 0)|0;
	return v|0;
    }

    // Load element i
    function do_load_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_load(i32a, i>>2)|0;
	return v|0;
    }

    // Store 37 in element 0
    function do_store() {
	var v = 0;
	v = atomic_store(i32a, 0, 37)|0;
	return v|0;
    }

    // Store 37 in element i
    function do_store_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_store(i32a, i>>2, 37)|0;
	return v|0;
    }

    // Add 37 to element 10
    function do_add() {
	var v = 0;
	v = atomic_add(i32a, 10, 37)|0;
	return v|0;
    }

    // Add 37 to element i
    function do_add_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_add(i32a, i>>2, 37)|0;
	return v|0;
    }

    // Subtract 148 from element 20
    function do_sub() {
	var v = 0;
	v = atomic_sub(i32a, 20, 148)|0;
	return v|0;
    }

    // Subtract 148 from element i
    function do_sub_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_sub(i32a, i>>2, 148)|0;
	return v|0;
    }

    // AND 0x33333333 into element 30
    function do_and() {
	var v = 0;
	v = atomic_and(i32a, 30, 0x33333333)|0;
	return v|0;
    }

    // AND 0x33333333 into element i
    function do_and_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_and(i32a, i>>2, 0x33333333)|0;
	return v|0;
    }

    // OR 0x33333333 into element 40
    function do_or() {
	var v = 0;
	v = atomic_or(i32a, 40, 0x33333333)|0;
	return v|0;
    }

    // OR 0x33333333 into element i
    function do_or_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_or(i32a, i>>2, 0x33333333)|0;
	return v|0;
    }

    // XOR 0x33333333 into element 50
    function do_xor() {
	var v = 0;
	v = atomic_xor(i32a, 50, 0x33333333)|0;
	return v|0;
    }

    // XOR 0x33333333 into element i
    function do_xor_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_xor(i32a, i>>2, 0x33333333)|0;
	return v|0;
    }

    // CAS element 100: 0 -> -1
    function do_cas1() {
	var v = 0;
	v = atomic_cmpxchg(i32a, 100, 0, -1)|0;
	return v|0;
    }

    // CAS element 100: -1 -> 0x5A5A5A5A
    function do_cas2() {
	var v = 0;
	v = atomic_cmpxchg(i32a, 100, -1, 0x5A5A5A5A)|0;
	return v|0;
    }

    // CAS element i: 0 -> -1
    function do_cas1_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i32a, i>>2, 0, -1)|0;
	return v|0;
    }

    // CAS element i: -1 -> 0x5A5A5A5A
    function do_cas2_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i32a, i>>2, -1, 0x5A5A5A5A)|0;
	return v|0;
    }

    return { fence: do_fence,
	     load: do_load,
	     load_i: do_load_i,
	     store: do_store,
	     store_i: do_store_i,
	     add: do_add,
	     add_i: do_add_i,
	     sub: do_sub,
	     sub_i: do_sub_i,
	     and: do_and,
	     and_i: do_and_i,
	     or: do_or,
	     or_i: do_or_i,
	     xor: do_xor,
	     xor_i: do_xor_i,
	     cas1: do_cas1,
	     cas2: do_cas2,
	     cas1_i: do_cas1_i,
	     cas2_i: do_cas2_i };
}

if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(loadModule_int32), true);

function test_int32(heap) {
    var i32a = new SharedInt32Array(heap);
    var i32m = loadModule_int32(this, {}, heap);

    var size = SharedInt32Array.BYTES_PER_ELEMENT;

    i32m.fence();

    i32a[0] = 12345;
    assertEq(i32m.load(), 12345);
    assertEq(i32m.load_i(size*0), 12345);

    assertEq(i32m.store(), 37);
    assertEq(i32a[0], 37);
    assertEq(i32m.store_i(size*0), 37);

    i32a[10] = 18;
    assertEq(i32m.add(), 18);
    assertEq(i32a[10], 18+37);
    assertEq(i32m.add_i(size*10), 18+37);
    assertEq(i32a[10], 18+37+37);

    i32a[20] = 4972;
    assertEq(i32m.sub(), 4972);
    assertEq(i32a[20], 4972 - 148);
    assertEq(i32m.sub_i(size*20), 4972 - 148);
    assertEq(i32a[20], 4972 - 148 - 148);

    i32a[30] = 0x66666666;
    assertEq(i32m.and(), 0x66666666);
    assertEq(i32a[30], 0x22222222);
    i32a[30] = 0x66666666;
    assertEq(i32m.and_i(size*30), 0x66666666);
    assertEq(i32a[30], 0x22222222);

    i32a[40] = 0x22222222;
    assertEq(i32m.or(), 0x22222222);
    assertEq(i32a[40], 0x33333333);
    i32a[40] = 0x22222222;
    assertEq(i32m.or_i(size*40), 0x22222222);
    assertEq(i32a[40], 0x33333333);

    i32a[50] = 0x22222222;
    assertEq(i32m.xor(), 0x22222222);
    assertEq(i32a[50], 0x11111111);
    i32a[50] = 0x22222222;
    assertEq(i32m.xor_i(size*50), 0x22222222);
    assertEq(i32a[50], 0x11111111);

    i32a[100] = 0;
    assertEq(i32m.cas1(), 0);
    assertEq(i32m.cas2(), -1);
    assertEq(i32a[100], 0x5A5A5A5A);

    i32a[100] = 0;
    assertEq(i32m.cas1_i(size*100), 0);
    assertEq(i32m.cas2_i(size*100), -1);
    assertEq(i32a[100], 0x5A5A5A5A);

    // Out-of-bounds accesses.

    var oob = (heap.byteLength * 2) & ~7;

    assertEq(i32m.cas1_i(oob), 0);
    assertEq(i32m.cas2_i(oob), 0);

    assertEq(i32m.or_i(oob), 0);
    assertEq(i32m.xor_i(oob), 0);
    assertEq(i32m.and_i(oob), 0);
    assertEq(i32m.add_i(oob), 0);
    assertEq(i32m.sub_i(oob), 0);
}

function loadModule_uint32(stdlib, foreign, heap) {
    "use asm";

    var atomic_fence = stdlib.Atomics.fence;
    var atomic_load = stdlib.Atomics.load;
    var atomic_store = stdlib.Atomics.store;
    var atomic_cmpxchg = stdlib.Atomics.compareExchange;
    var atomic_add = stdlib.Atomics.add;
    var atomic_sub = stdlib.Atomics.sub;
    var atomic_and = stdlib.Atomics.and;
    var atomic_or = stdlib.Atomics.or;
    var atomic_xor = stdlib.Atomics.xor;

    var i32a = new stdlib.SharedUint32Array(heap);

    // Load element 0
    function do_load() {
	var v = 0;
	v = atomic_load(i32a, 0)|0;
	return +(v>>>0);
    }

    // Load element i
    function do_load_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_load(i32a, i>>2)|0;
	return +(v>>>0);
    }

    // Store 37 in element 0
    function do_store() {
	var v = 0;
	v = atomic_store(i32a, 0, 37)|0;
	return +(v>>>0);
    }

    // Store 37 in element i
    function do_store_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_store(i32a, i>>2, 37)|0;
	return +(v>>>0);
    }

    // Add 37 to element 10
    function do_add() {
	var v = 0;
	v = atomic_add(i32a, 10, 37)|0;
	return +(v>>>0);
    }

    // Add 37 to element i
    function do_add_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_add(i32a, i>>2, 37)|0;
	return +(v>>>0);
    }

    // Subtract 148 from element 20
    function do_sub() {
	var v = 0;
	v = atomic_sub(i32a, 20, 148)|0;
	return +(v>>>0);
    }

    // Subtract 148 from element i
    function do_sub_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_sub(i32a, i>>2, 148)|0;
	return +(v>>>0);
    }

    // AND 0x33333333 into element 30
    function do_and() {
	var v = 0;
	v = atomic_and(i32a, 30, 0x33333333)|0;
	return +(v>>>0);
    }

    // AND 0x33333333 into element i
    function do_and_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_and(i32a, i>>2, 0x33333333)|0;
	return +(v>>>0);
    }

    // OR 0x33333333 into element 40
    function do_or() {
	var v = 0;
	v = atomic_or(i32a, 40, 0x33333333)|0;
	return +(v>>>0);
    }

    // OR 0x33333333 into element i
    function do_or_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_or(i32a, i>>2, 0x33333333)|0;
	return +(v>>>0);
    }

    // XOR 0x33333333 into element 50
    function do_xor() {
	var v = 0;
	v = atomic_xor(i32a, 50, 0x33333333)|0;
	return +(v>>>0);
    }

    // XOR 0x33333333 into element i
    function do_xor_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_xor(i32a, i>>2, 0x33333333)|0;
	return +(v>>>0);
    }

    // CAS element 100: 0 -> -1
    function do_cas1() {
	var v = 0;
	v = atomic_cmpxchg(i32a, 100, 0, -1)|0;
	return +(v>>>0);
    }

    // CAS element 100: -1 -> 0x5A5A5A5A
    function do_cas2() {
	var v = 0;
	v = atomic_cmpxchg(i32a, 100, -1, 0x5A5A5A5A)|0;
	return +(v>>>0);
    }

    // CAS element i: 0 -> -1
    function do_cas1_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i32a, i>>2, 0, -1)|0;
	return +(v>>>0);
    }

    // CAS element i: -1 -> 0x5A5A5A5A
    function do_cas2_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i32a, i>>2, -1, 0x5A5A5A5A)|0;
	return +(v>>>0);
    }

    return { load: do_load,
	     load_i: do_load_i,
	     store: do_store,
	     store_i: do_store_i,
	     add: do_add,
	     add_i: do_add_i,
	     sub: do_sub,
	     sub_i: do_sub_i,
	     and: do_and,
	     and_i: do_and_i,
	     or: do_or,
	     or_i: do_or_i,
	     xor: do_xor,
	     xor_i: do_xor_i,
	     cas1: do_cas1,
	     cas2: do_cas2,
	     cas1_i: do_cas1_i,
	     cas2_i: do_cas2_i };
}

if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(loadModule_uint32), true);

function test_uint32(heap) {
    var i32a = new SharedUint32Array(heap);
    var i32m = loadModule_uint32(this, {}, heap);

    var size = SharedUint32Array.BYTES_PER_ELEMENT;

    i32a[0] = 12345;
    assertEq(i32m.load(), 12345);
    assertEq(i32m.load_i(size*0), 12345);

    assertEq(i32m.store(), 37);
    assertEq(i32a[0], 37);
    assertEq(i32m.store_i(size*0), 37);

    i32a[10] = 18;
    assertEq(i32m.add(), 18);
    assertEq(i32a[10], 18+37);
    assertEq(i32m.add_i(size*10), 18+37);
    assertEq(i32a[10], 18+37+37);

    i32a[20] = 4972;
    assertEq(i32m.sub(), 4972);
    assertEq(i32a[20], 4972 - 148);
    assertEq(i32m.sub_i(size*20), 4972 - 148);
    assertEq(i32a[20], 4972 - 148 - 148);

    i32a[30] = 0x66666666;
    assertEq(i32m.and(), 0x66666666);
    assertEq(i32a[30], 0x22222222);
    i32a[30] = 0x66666666;
    assertEq(i32m.and_i(size*30), 0x66666666);
    assertEq(i32a[30], 0x22222222);

    i32a[40] = 0x22222222;
    assertEq(i32m.or(), 0x22222222);
    assertEq(i32a[40], 0x33333333);
    i32a[40] = 0x22222222;
    assertEq(i32m.or_i(size*40), 0x22222222);
    assertEq(i32a[40], 0x33333333);

    i32a[50] = 0x22222222;
    assertEq(i32m.xor(), 0x22222222);
    assertEq(i32a[50], 0x11111111);
    i32a[50] = 0x22222222;
    assertEq(i32m.xor_i(size*50), 0x22222222);
    assertEq(i32a[50], 0x11111111);

    i32a[100] = 0;
    assertEq(i32m.cas1(), 0);
    assertEq(i32m.cas2(), 0xFFFFFFFF);
    assertEq(i32a[100], 0x5A5A5A5A);

    i32a[100] = 0;
    assertEq(i32m.cas1_i(size*100), 0);
    assertEq(i32m.cas2_i(size*100), 0xFFFFFFFF);
    assertEq(i32a[100], 0x5A5A5A5A);

    // Out-of-bounds accesses.

    var oob = (heap.byteLength * 2) & ~7;

    assertEq(i32m.cas1_i(oob), 0);
    assertEq(i32m.cas2_i(oob), 0);

    assertEq(i32m.or_i(oob), 0);
    assertEq(i32m.xor_i(oob), 0);
    assertEq(i32m.and_i(oob), 0);
    assertEq(i32m.add_i(oob), 0);
    assertEq(i32m.sub_i(oob), 0);
}

function loadModule_int16(stdlib, foreign, heap) {
    "use asm";

    var atomic_fence = stdlib.Atomics.fence;
    var atomic_load = stdlib.Atomics.load;
    var atomic_store = stdlib.Atomics.store;
    var atomic_cmpxchg = stdlib.Atomics.compareExchange;
    var atomic_add = stdlib.Atomics.add;
    var atomic_sub = stdlib.Atomics.sub;
    var atomic_and = stdlib.Atomics.and;
    var atomic_or = stdlib.Atomics.or;
    var atomic_xor = stdlib.Atomics.xor;

    var i16a = new stdlib.SharedInt16Array(heap);

    function do_fence() {
	atomic_fence();
    }

    // Load element 0
    function do_load() {
	var v = 0;
	v = atomic_load(i16a, 0)|0;
	return v|0;
    }

    // Load element i
    function do_load_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_load(i16a, i>>1)|0;
	return v|0;
    }

    // Store 37 in element 0
    function do_store() {
	var v = 0;
	v = atomic_store(i16a, 0, 37)|0;
	return v|0;
    }

    // Store 37 in element i
    function do_store_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_store(i16a, i>>1, 37)|0;
	return v|0;
    }

    // Add 37 to element 10
    function do_add() {
	var v = 0;
	v = atomic_add(i16a, 10, 37)|0;
	return v|0;
    }

    // Add 37 to element i
    function do_add_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_add(i16a, i>>1, 37)|0;
	return v|0;
    }

    // Subtract 148 from element 20
    function do_sub() {
	var v = 0;
	v = atomic_sub(i16a, 20, 148)|0;
	return v|0;
    }

    // Subtract 148 from element i
    function do_sub_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_sub(i16a, i>>1, 148)|0;
	return v|0;
    }

    // AND 0x3333 into element 30
    function do_and() {
	var v = 0;
	v = atomic_and(i16a, 30, 0x3333)|0;
	return v|0;
    }

    // AND 0x3333 into element i
    function do_and_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_and(i16a, i>>1, 0x3333)|0;
	return v|0;
    }

    // OR 0x3333 into element 40
    function do_or() {
	var v = 0;
	v = atomic_or(i16a, 40, 0x3333)|0;
	return v|0;
    }

    // OR 0x3333 into element i
    function do_or_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_or(i16a, i>>1, 0x3333)|0;
	return v|0;
    }

    // XOR 0x3333 into element 50
    function do_xor() {
	var v = 0;
	v = atomic_xor(i16a, 50, 0x3333)|0;
	return v|0;
    }

    // XOR 0x3333 into element i
    function do_xor_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_xor(i16a, i>>1, 0x3333)|0;
	return v|0;
    }

    // CAS element 100: 0 -> -1
    function do_cas1() {
	var v = 0;
	v = atomic_cmpxchg(i16a, 100, 0, -1)|0;
	return v|0;
    }

    // CAS element 100: -1 -> 0x5A5A
    function do_cas2() {
	var v = 0;
	v = atomic_cmpxchg(i16a, 100, -1, 0x5A5A)|0;
	return v|0;
    }

    // CAS element i: 0 -> -1
    function do_cas1_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i16a, i>>1, 0, -1)|0;
	return v|0;
    }

    // CAS element i: -1 -> 0x5A5A
    function do_cas2_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i16a, i>>1, -1, 0x5A5A)|0;
	return v|0;
    }

    return { fence: do_fence,
	     load: do_load,
	     load_i: do_load_i,
	     store: do_store,
	     store_i: do_store_i,
	     add: do_add,
	     add_i: do_add_i,
	     sub: do_sub,
	     sub_i: do_sub_i,
	     and: do_and,
	     and_i: do_and_i,
	     or: do_or,
	     or_i: do_or_i,
	     xor: do_xor,
	     xor_i: do_xor_i,
	     cas1: do_cas1,
	     cas2: do_cas2,
	     cas1_i: do_cas1_i,
	     cas2_i: do_cas2_i };
}

if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(loadModule_int16), true);

function test_int16(heap) {
    var i16a = new SharedInt16Array(heap);
    var i16m = loadModule_int16(this, {}, heap);

    var size = SharedInt16Array.BYTES_PER_ELEMENT;

    i16m.fence();

    i16a[0] = 12345;
    assertEq(i16m.load(), 12345);
    assertEq(i16m.load_i(size*0), 12345);

    i16a[0] = -38;
    assertEq(i16m.load(), -38);
    assertEq(i16m.load_i(size*0), -38);

    assertEq(i16m.store(), 37);
    assertEq(i16a[0], 37);
    assertEq(i16m.store_i(size*0), 37);

    i16a[10] = 18;
    assertEq(i16m.add(), 18);
    assertEq(i16a[10], 18+37);
    assertEq(i16m.add_i(size*10), 18+37);
    assertEq(i16a[10], 18+37+37);

    i16a[10] = -38;
    assertEq(i16m.add(), -38);
    assertEq(i16a[10], -38+37);
    assertEq(i16m.add_i(size*10), -38+37);
    assertEq(i16a[10], -38+37+37);

    i16a[20] = 4972;
    assertEq(i16m.sub(), 4972);
    assertEq(i16a[20], 4972 - 148);
    assertEq(i16m.sub_i(size*20), 4972 - 148);
    assertEq(i16a[20], 4972 - 148 - 148);

    i16a[30] = 0x6666;
    assertEq(i16m.and(), 0x6666);
    assertEq(i16a[30], 0x2222);
    i16a[30] = 0x6666;
    assertEq(i16m.and_i(size*30), 0x6666);
    assertEq(i16a[30], 0x2222);

    i16a[40] = 0x2222;
    assertEq(i16m.or(), 0x2222);
    assertEq(i16a[40], 0x3333);
    i16a[40] = 0x2222;
    assertEq(i16m.or_i(size*40), 0x2222);
    assertEq(i16a[40], 0x3333);

    i16a[50] = 0x2222;
    assertEq(i16m.xor(), 0x2222);
    assertEq(i16a[50], 0x1111);
    i16a[50] = 0x2222;
    assertEq(i16m.xor_i(size*50), 0x2222);
    assertEq(i16a[50], 0x1111);

    i16a[100] = 0;
    assertEq(i16m.cas1(), 0);
    assertEq(i16m.cas2(), -1);
    assertEq(i16a[100], 0x5A5A);

    i16a[100] = 0;
    assertEq(i16m.cas1_i(size*100), 0);
    assertEq(i16m.cas2_i(size*100), -1);
    assertEq(i16a[100], 0x5A5A);

    var oob = (heap.byteLength * 2) & ~7;

    assertEq(i16m.cas1_i(oob), 0);
    assertEq(i16m.cas2_i(oob), 0);

    assertEq(i16m.or_i(oob), 0);
    assertEq(i16m.xor_i(oob), 0);
    assertEq(i16m.and_i(oob), 0);
    assertEq(i16m.add_i(oob), 0);
    assertEq(i16m.sub_i(oob), 0);
}

function loadModule_uint16(stdlib, foreign, heap) {
    "use asm";

    var atomic_load = stdlib.Atomics.load;
    var atomic_store = stdlib.Atomics.store;
    var atomic_cmpxchg = stdlib.Atomics.compareExchange;
    var atomic_add = stdlib.Atomics.add;
    var atomic_sub = stdlib.Atomics.sub;
    var atomic_and = stdlib.Atomics.and;
    var atomic_or = stdlib.Atomics.or;
    var atomic_xor = stdlib.Atomics.xor;

    var i16a = new stdlib.SharedUint16Array(heap);

    // Load element 0
    function do_load() {
	var v = 0;
	v = atomic_load(i16a, 0)|0;
	return v|0;
    }

    // Load element i
    function do_load_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_load(i16a, i>>1)|0;
	return v|0;
    }

    // Store 37 in element 0
    function do_store() {
	var v = 0;
	v = atomic_store(i16a, 0, 37)|0;
	return v|0;
    }

    // Store 37 in element i
    function do_store_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_store(i16a, i>>1, 37)|0;
	return v|0;
    }

    // Add 37 to element 10
    function do_add() {
	var v = 0;
	v = atomic_add(i16a, 10, 37)|0;
	return v|0;
    }

    // Add 37 to element i
    function do_add_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_add(i16a, i>>1, 37)|0;
	return v|0;
    }

    // Subtract 148 from element 20
    function do_sub() {
	var v = 0;
	v = atomic_sub(i16a, 20, 148)|0;
	return v|0;
    }

    // Subtract 148 from element i
    function do_sub_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_sub(i16a, i>>1, 148)|0;
	return v|0;
    }

    // AND 0x3333 into element 30
    function do_and() {
	var v = 0;
	v = atomic_and(i16a, 30, 0x3333)|0;
	return v|0;
    }

    // AND 0x3333 into element i
    function do_and_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_and(i16a, i>>1, 0x3333)|0;
	return v|0;
    }

    // OR 0x3333 into element 40
    function do_or() {
	var v = 0;
	v = atomic_or(i16a, 40, 0x3333)|0;
	return v|0;
    }

    // OR 0x3333 into element i
    function do_or_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_or(i16a, i>>1, 0x3333)|0;
	return v|0;
    }

    // XOR 0x3333 into element 50
    function do_xor() {
	var v = 0;
	v = atomic_xor(i16a, 50, 0x3333)|0;
	return v|0;
    }

    // XOR 0x3333 into element i
    function do_xor_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_xor(i16a, i>>1, 0x3333)|0;
	return v|0;
    }

    // CAS element 100: 0 -> -1
    function do_cas1() {
	var v = 0;
	v = atomic_cmpxchg(i16a, 100, 0, -1)|0;
	return v|0;
    }

    // CAS element 100: -1 -> 0x5A5A
    function do_cas2() {
	var v = 0;
	v = atomic_cmpxchg(i16a, 100, -1, 0x5A5A)|0;
	return v|0;
    }

    // CAS element i: 0 -> -1
    function do_cas1_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i16a, i>>1, 0, -1)|0;
	return v|0;
    }

    // CAS element i: -1 -> 0x5A5A
    function do_cas2_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i16a, i>>1, -1, 0x5A5A)|0;
	return v|0;
    }

    return { load: do_load,
	     load_i: do_load_i,
	     store: do_store,
	     store_i: do_store_i,
	     add: do_add,
	     add_i: do_add_i,
	     sub: do_sub,
	     sub_i: do_sub_i,
	     and: do_and,
	     and_i: do_and_i,
	     or: do_or,
	     or_i: do_or_i,
	     xor: do_xor,
	     xor_i: do_xor_i,
	     cas1: do_cas1,
	     cas2: do_cas2,
	     cas1_i: do_cas1_i,
	     cas2_i: do_cas2_i };
}

if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(loadModule_uint16), true);

function test_uint16(heap) {
    var i16a = new SharedUint16Array(heap);
    var i16m = loadModule_uint16(this, {}, heap);

    var size = SharedUint16Array.BYTES_PER_ELEMENT;

    i16a[0] = 12345;
    assertEq(i16m.load(), 12345);
    assertEq(i16m.load_i(size*0), 12345);

    i16a[0] = -38;
    assertEq(i16m.load(), (0x10000-38));
    assertEq(i16m.load_i(size*0), (0x10000-38));

    assertEq(i16m.store(), 37);
    assertEq(i16a[0], 37);
    assertEq(i16m.store_i(size*0), 37);

    i16a[10] = 18;
    assertEq(i16m.add(), 18);
    assertEq(i16a[10], 18+37);
    assertEq(i16m.add_i(size*10), 18+37);
    assertEq(i16a[10], 18+37+37);

    i16a[10] = -38;
    assertEq(i16m.add(), (0x10000-38));
    assertEq(i16a[10], (0x10000-38)+37);
    assertEq(i16m.add_i(size*10), (0x10000-38)+37);
    assertEq(i16a[10], ((0x10000-38)+37+37) & 0xFFFF);

    i16a[20] = 4972;
    assertEq(i16m.sub(), 4972);
    assertEq(i16a[20], 4972 - 148);
    assertEq(i16m.sub_i(size*20), 4972 - 148);
    assertEq(i16a[20], 4972 - 148 - 148);

    i16a[30] = 0x6666;
    assertEq(i16m.and(), 0x6666);
    assertEq(i16a[30], 0x2222);
    i16a[30] = 0x6666;
    assertEq(i16m.and_i(size*30), 0x6666);
    assertEq(i16a[30], 0x2222);

    i16a[40] = 0x2222;
    assertEq(i16m.or(), 0x2222);
    assertEq(i16a[40], 0x3333);
    i16a[40] = 0x2222;
    assertEq(i16m.or_i(size*40), 0x2222);
    assertEq(i16a[40], 0x3333);

    i16a[50] = 0x2222;
    assertEq(i16m.xor(), 0x2222);
    assertEq(i16a[50], 0x1111);
    i16a[50] = 0x2222;
    assertEq(i16m.xor_i(size*50), 0x2222);
    assertEq(i16a[50], 0x1111);

    i16a[100] = 0;
    assertEq(i16m.cas1(), 0);
    assertEq(i16m.cas2(), -1 & 0xFFFF);
    assertEq(i16a[100], 0x5A5A);

    i16a[100] = 0;
    assertEq(i16m.cas1_i(size*100), 0);
    assertEq(i16m.cas2_i(size*100), -1 & 0xFFFF);
    assertEq(i16a[100], 0x5A5A);

    var oob = (heap.byteLength * 2) & ~7;

    assertEq(i16m.cas1_i(oob), 0);
    assertEq(i16m.cas2_i(oob), 0);

    assertEq(i16m.or_i(oob), 0);
    assertEq(i16m.xor_i(oob), 0);
    assertEq(i16m.and_i(oob), 0);
    assertEq(i16m.add_i(oob), 0);
    assertEq(i16m.sub_i(oob), 0);
}

function loadModule_int8(stdlib, foreign, heap) {
    "use asm";

    var atomic_load = stdlib.Atomics.load;
    var atomic_store = stdlib.Atomics.store;
    var atomic_cmpxchg = stdlib.Atomics.compareExchange;
    var atomic_add = stdlib.Atomics.add;
    var atomic_sub = stdlib.Atomics.sub;
    var atomic_and = stdlib.Atomics.and;
    var atomic_or = stdlib.Atomics.or;
    var atomic_xor = stdlib.Atomics.xor;

    var i8a = new stdlib.SharedInt8Array(heap);

    // Load element 0
    function do_load() {
	var v = 0;
	v = atomic_load(i8a, 0)|0;
	return v|0;
    }

    // Load element i
    function do_load_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_load(i8a, i)|0;
	return v|0;
    }

    // Store 37 in element 0
    function do_store() {
	var v = 0;
	v = atomic_store(i8a, 0, 37)|0;
	return v|0;
    }

    // Store 37 in element i
    function do_store_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_store(i8a, i, 37)|0;
	return v|0;
    }

    // Add 37 to element 10
    function do_add() {
	var v = 0;
	v = atomic_add(i8a, 10, 37)|0;
	return v|0;
    }

    // Add 37 to element i
    function do_add_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_add(i8a, i, 37)|0;
	return v|0;
    }

    // Subtract 108 from element 20
    function do_sub() {
	var v = 0;
	v = atomic_sub(i8a, 20, 108)|0;
	return v|0;
    }

    // Subtract 108 from element i
    function do_sub_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_sub(i8a, i, 108)|0;
	return v|0;
    }

    // AND 0x33 into element 30
    function do_and() {
	var v = 0;
	v = atomic_and(i8a, 30, 0x33)|0;
	return v|0;
    }

    // AND 0x33 into element i
    function do_and_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_and(i8a, i, 0x33)|0;
	return v|0;
    }

    // OR 0x33 into element 40
    function do_or() {
	var v = 0;
	v = atomic_or(i8a, 40, 0x33)|0;
	return v|0;
    }

    // OR 0x33 into element i
    function do_or_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_or(i8a, i, 0x33)|0;
	return v|0;
    }

    // XOR 0x33 into element 50
    function do_xor() {
	var v = 0;
	v = atomic_xor(i8a, 50, 0x33)|0;
	return v|0;
    }

    // XOR 0x33 into element i
    function do_xor_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_xor(i8a, i, 0x33)|0;
	return v|0;
    }

    // CAS element 100: 0 -> -1
    function do_cas1() {
	var v = 0;
	v = atomic_cmpxchg(i8a, 100, 0, -1)|0;
	return v|0;
    }

    // CAS element 100: -1 -> 0x5A
    function do_cas2() {
	var v = 0;
	v = atomic_cmpxchg(i8a, 100, -1, 0x5A)|0;
	return v|0;
    }

    // CAS element i: 0 -> -1
    function do_cas1_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i8a, i, 0, -1)|0;
	return v|0;
    }

    // CAS element i: -1 -> 0x5A
    function do_cas2_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i8a, i, -1, 0x5A)|0;
	return v|0;
    }

    return { load: do_load,
	     load_i: do_load_i,
	     store: do_store,
	     store_i: do_store_i,
	     add: do_add,
	     add_i: do_add_i,
	     sub: do_sub,
	     sub_i: do_sub_i,
	     and: do_and,
	     and_i: do_and_i,
	     or: do_or,
	     or_i: do_or_i,
	     xor: do_xor,
	     xor_i: do_xor_i,
	     cas1: do_cas1,
	     cas2: do_cas2,
	     cas1_i: do_cas1_i,
	     cas2_i: do_cas2_i };
}

if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(loadModule_int8), true);

function test_int8(heap) {
    var i8a = new SharedInt8Array(heap);
    var i8m = loadModule_int8(this, {}, heap);

    for ( var i=0 ; i < i8a.length ; i++ )
	i8a[i] = 0;

    var size = SharedInt8Array.BYTES_PER_ELEMENT;

    i8a[0] = 123;
    assertEq(i8m.load(), 123);
    assertEq(i8m.load_i(0), 123);

    assertEq(i8m.store(), 37);
    assertEq(i8a[0], 37);
    assertEq(i8m.store_i(0), 37);

    i8a[10] = 18;
    assertEq(i8m.add(), 18);
    assertEq(i8a[10], 18+37);
    assertEq(i8m.add_i(10), 18+37);
    assertEq(i8a[10], 18+37+37);

    i8a[20] = 49;
    assertEq(i8m.sub(), 49);
    assertEq(i8a[20], 49 - 108);
    assertEq(i8m.sub_i(20), 49 - 108);
    assertEq(i8a[20], ((49 - 108 - 108) << 24) >> 24); // Byte, sign extended

    i8a[30] = 0x66;
    assertEq(i8m.and(), 0x66);
    assertEq(i8a[30], 0x22);
    i8a[30] = 0x66;
    assertEq(i8m.and_i(30), 0x66);
    assertEq(i8a[30], 0x22);

    i8a[40] = 0x22;
    assertEq(i8m.or(), 0x22);
    assertEq(i8a[40], 0x33);
    i8a[40] = 0x22;
    assertEq(i8m.or_i(40), 0x22);
    assertEq(i8a[40], 0x33);

    i8a[50] = 0x22;
    assertEq(i8m.xor(), 0x22);
    assertEq(i8a[50], 0x11);
    i8a[50] = 0x22;
    assertEq(i8m.xor_i(50), 0x22);
    assertEq(i8a[50], 0x11);

    i8a[100] = 0;
    assertEq(i8m.cas1(), 0);
    assertEq(i8m.cas2(), -1);
    assertEq(i8a[100], 0x5A);

    i8a[100] = 0;
    assertEq(i8m.cas1_i(100), 0);
    assertEq(i8m.cas2_i(100), -1);
    assertEq(i8a[100], 0x5A);

    var oob = (heap.byteLength * 2) & ~7;

    assertEq(i8m.cas1_i(oob), 0);
    assertEq(i8m.cas2_i(oob), 0);

    assertEq(i8m.or_i(oob), 0);
    assertEq(i8m.xor_i(oob), 0);
    assertEq(i8m.and_i(oob), 0);
    assertEq(i8m.add_i(oob), 0);
    assertEq(i8m.sub_i(oob), 0);
}

function loadModule_uint8(stdlib, foreign, heap) {
    "use asm";

    var atomic_load = stdlib.Atomics.load;
    var atomic_store = stdlib.Atomics.store;
    var atomic_cmpxchg = stdlib.Atomics.compareExchange;
    var atomic_add = stdlib.Atomics.add;
    var atomic_sub = stdlib.Atomics.sub;
    var atomic_and = stdlib.Atomics.and;
    var atomic_or = stdlib.Atomics.or;
    var atomic_xor = stdlib.Atomics.xor;

    var i8a = new stdlib.SharedUint8Array(heap);

    // Load element 0
    function do_load() {
	var v = 0;
	v = atomic_load(i8a, 0)|0;
	return v|0;
    }

    // Load element i
    function do_load_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_load(i8a, i)|0;
	return v|0;
    }

    // Store 37 in element 0
    function do_store() {
	var v = 0;
	v = atomic_store(i8a, 0, 37)|0;
	return v|0;
    }

    // Store 37 in element i
    function do_store_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_store(i8a, i, 37)|0;
	return v|0;
    }

    // Add 37 to element 10
    function do_add() {
	var v = 0;
	v = atomic_add(i8a, 10, 37)|0;
	return v|0;
    }

    // Add 37 to element i
    function do_add_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_add(i8a, i, 37)|0;
	return v|0;
    }

    // Subtract 108 from element 20
    function do_sub() {
	var v = 0;
	v = atomic_sub(i8a, 20, 108)|0;
	return v|0;
    }

    // Subtract 108 from element i
    function do_sub_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_sub(i8a, i, 108)|0;
	return v|0;
    }

    // AND 0x33 into element 30
    function do_and() {
	var v = 0;
	v = atomic_and(i8a, 30, 0x33)|0;
	return v|0;
    }

    // AND 0x33 into element i
    function do_and_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_and(i8a, i, 0x33)|0;
	return v|0;
    }

    // OR 0x33 into element 40
    function do_or() {
	var v = 0;
	v = atomic_or(i8a, 40, 0x33)|0;
	return v|0;
    }

    // OR 0x33 into element i
    function do_or_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_or(i8a, i, 0x33)|0;
	return v|0;
    }

    // XOR 0x33 into element 50
    function do_xor() {
	var v = 0;
	v = atomic_xor(i8a, 50, 0x33)|0;
	return v|0;
    }

    // XOR 0x33 into element i
    function do_xor_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_xor(i8a, i, 0x33)|0;
	return v|0;
    }

    // CAS element 100: 0 -> -1
    function do_cas1() {
	var v = 0;
	v = atomic_cmpxchg(i8a, 100, 0, -1)|0;
	return v|0;
    }

    // CAS element 100: -1 -> 0x5A
    function do_cas2() {
	var v = 0;
	v = atomic_cmpxchg(i8a, 100, -1, 0x5A)|0;
	return v|0;
    }

    // CAS element i: 0 -> -1
    function do_cas1_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i8a, i, 0, -1)|0;
	return v|0;
    }

    // CAS element i: -1 -> 0x5A
    function do_cas2_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i8a, i, -1, 0x5A)|0;
	return v|0;
    }

    return { load: do_load,
	     load_i: do_load_i,
	     store: do_store,
	     store_i: do_store_i,
	     add: do_add,
	     add_i: do_add_i,
	     sub: do_sub,
	     sub_i: do_sub_i,
	     and: do_and,
	     and_i: do_and_i,
	     or: do_or,
	     or_i: do_or_i,
	     xor: do_xor,
	     xor_i: do_xor_i,
	     cas1: do_cas1,
	     cas2: do_cas2,
	     cas1_i: do_cas1_i,
	     cas2_i: do_cas2_i };
}

if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(loadModule_uint8), true);

function test_uint8(heap) {
    var i8a = new SharedUint8Array(heap);
    var i8m = loadModule_uint8(this, {}, heap);

    for ( var i=0 ; i < i8a.length ; i++ )
	i8a[i] = 0;

    var size = SharedUint8Array.BYTES_PER_ELEMENT;

    i8a[0] = 123;
    assertEq(i8m.load(), 123);
    assertEq(i8m.load_i(0), 123);

    i8a[0] = -38;
    assertEq(i8m.load(), (0x100-38));
    assertEq(i8m.load_i(size*0), (0x100-38));

    assertEq(i8m.store(), 37);
    assertEq(i8a[0], 37);
    assertEq(i8m.store_i(0), 37);

    i8a[10] = 18;
    assertEq(i8m.add(), 18);
    assertEq(i8a[10], 18+37);
    assertEq(i8m.add_i(10), 18+37);
    assertEq(i8a[10], 18+37+37);

    i8a[10] = -38;
    assertEq(i8m.add(), (0x100-38));
    assertEq(i8a[10], (0x100-38)+37);
    assertEq(i8m.add_i(size*10), (0x100-38)+37);
    assertEq(i8a[10], ((0x100-38)+37+37) & 0xFF);

    i8a[20] = 49;
    assertEq(i8m.sub(), 49);
    assertEq(i8a[20], (49 - 108) & 255);
    assertEq(i8m.sub_i(20), (49 - 108) & 255);
    assertEq(i8a[20], (49 - 108 - 108) & 255); // Byte, zero extended

    i8a[30] = 0x66;
    assertEq(i8m.and(), 0x66);
    assertEq(i8a[30], 0x22);
    i8a[30] = 0x66;
    assertEq(i8m.and_i(30), 0x66);
    assertEq(i8a[30], 0x22);

    i8a[40] = 0x22;
    assertEq(i8m.or(), 0x22);
    assertEq(i8a[40], 0x33);
    i8a[40] = 0x22;
    assertEq(i8m.or_i(40), 0x22);
    assertEq(i8a[40], 0x33);

    i8a[50] = 0x22;
    assertEq(i8m.xor(), 0x22);
    assertEq(i8a[50], 0x11);
    i8a[50] = 0x22;
    assertEq(i8m.xor_i(50), 0x22);
    assertEq(i8a[50], 0x11);

    i8a[100] = 0;
    assertEq(i8m.cas1(), 0);
    assertEq(i8m.cas2(), 255);
    assertEq(i8a[100], 0x5A);

    i8a[100] = 0;
    assertEq(i8m.cas1_i(100), 0);
    assertEq(i8m.cas2_i(100), 255);
    assertEq(i8a[100], 0x5A);

    var oob = (heap.byteLength * 2) & ~7;

    assertEq(i8m.cas1_i(oob), 0);
    assertEq(i8m.cas2_i(oob), 0);

    assertEq(i8m.or_i(oob), 0);
    assertEq(i8m.xor_i(oob), 0);
    assertEq(i8m.and_i(oob), 0);
    assertEq(i8m.add_i(oob), 0);
    assertEq(i8m.sub_i(oob), 0);
}

function loadModule_misc(stdlib, foreign, heap) {
    "use asm";

    var atomic_isLockFree = stdlib.Atomics.isLockFree;

    function ilf1() {
	return atomic_isLockFree(1)|0;
    }

    function ilf2() {
	return atomic_isLockFree(2)|0;
    }

    function ilf3() {
	return atomic_isLockFree(3)|0;
    }

    function ilf4() {
	return atomic_isLockFree(4)|0;
    }

    function ilf5() {
	return atomic_isLockFree(5)|0;
    }

    function ilf6() {
	return atomic_isLockFree(6)|0;
    }

    function ilf7() {
	return atomic_isLockFree(7)|0;
    }

    function ilf8() {
	return atomic_isLockFree(8)|0;
    }

    function ilf9() {
	return atomic_isLockFree(9)|0;
    }

    return { ilf1: ilf1,
	     ilf2: ilf2,
	     ilf3: ilf3,
	     ilf4: ilf4,
	     ilf5: ilf5,
	     ilf6: ilf6,
	     ilf7: ilf7,
	     ilf8: ilf8,
	     ilf9: ilf9 };
}

if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(loadModule_misc), true);

function test_misc(heap) {
    var misc = loadModule_misc(this, {}, heap);

    assertEq(misc.ilf1(), 1);
    assertEq(misc.ilf2(), 1);
    assertEq(misc.ilf3(), 0);
    assertEq(misc.ilf4(), 1);
    assertEq(misc.ilf5(), 0);
    assertEq(misc.ilf6(), 0);
    assertEq(misc.ilf7(), 0);
    var v = misc.ilf8();
    assertEq(v === 0 || v === 1, true);
    assertEq(misc.ilf9(), 0);
}

// SharedUint8ClampedArray is not supported for asm.js.

var heap = new SharedArrayBuffer(65536);

test_int8(heap);
test_uint8(heap);
test_int16(heap);
test_uint16(heap);
test_int32(heap);
test_uint32(heap);
test_misc(heap);
