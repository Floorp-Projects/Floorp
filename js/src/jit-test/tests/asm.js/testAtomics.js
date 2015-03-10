// |jit-test| test-also-noasmjs
if (!this.SharedArrayBuffer || !this.SharedInt32Array || !this.Atomics)
    quit();

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
	v = atomic_load(i32a, 0);
	return v|0;
    }

    // Load element i
    function do_load_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_load(i32a, i>>2);
	return v|0;
    }

    // Store 37 in element 0
    function do_store() {
	var v = 0;
	v = atomic_store(i32a, 0, 37);
	return v|0;
    }

    // Store 37 in element i
    function do_store_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_store(i32a, i>>2, 37);
	return v|0;
    }

    // Add 37 to element 10
    function do_add() {
	var v = 0;
	v = atomic_add(i32a, 10, 37);
	return v|0;
    }

    // Add 37 to element i
    function do_add_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_add(i32a, i>>2, 37);
	return v|0;
    }

    // Subtract 148 from element 20
    function do_sub() {
	var v = 0;
	v = atomic_sub(i32a, 20, 148);
	return v|0;
    }

    // Subtract 148 from element i
    function do_sub_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_sub(i32a, i>>2, 148);
	return v|0;
    }

    // AND 0x33333333 into element 30
    function do_and() {
	var v = 0;
	v = atomic_and(i32a, 30, 0x33333333);
	return v|0;
    }

    // AND 0x33333333 into element i
    function do_and_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_and(i32a, i>>2, 0x33333333);
	return v|0;
    }

    // OR 0x33333333 into element 40
    function do_or() {
	var v = 0;
	v = atomic_or(i32a, 40, 0x33333333);
	return v|0;
    }

    // OR 0x33333333 into element i
    function do_or_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_or(i32a, i>>2, 0x33333333);
	return v|0;
    }

    // XOR 0x33333333 into element 50
    function do_xor() {
	var v = 0;
	v = atomic_xor(i32a, 50, 0x33333333);
	return v|0;
    }

    // XOR 0x33333333 into element i
    function do_xor_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_xor(i32a, i>>2, 0x33333333);
	return v|0;
    }

    // CAS element 100: 0 -> -1
    function do_cas1() {
	var v = 0;
	v = atomic_cmpxchg(i32a, 100, 0, -1);
	return v|0;
    }

    // CAS element 100: -1 -> 0x5A5A5A5A
    function do_cas2() {
	var v = 0;
	v = atomic_cmpxchg(i32a, 100, -1, 0x5A5A5A5A);
	return v|0;
    }

    // CAS element i: 0 -> -1
    function do_cas1_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i32a, i>>2, 0, -1);
	return v|0;
    }

    // CAS element i: -1 -> 0x5A5A5A5A
    function do_cas2_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i32a, i>>2, -1, 0x5A5A5A5A);
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
	v = atomic_load(i8a, 0);
	return v|0;
    }

    // Load element i
    function do_load_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_load(i8a, i);
	return v|0;
    }

    // Store 37 in element 0
    function do_store() {
	var v = 0;
	v = atomic_store(i8a, 0, 37);
	return v|0;
    }

    // Store 37 in element i
    function do_store_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_store(i8a, i, 37);
	return v|0;
    }

    // Add 37 to element 10
    function do_add() {
	var v = 0;
	v = atomic_add(i8a, 10, 37);
	return v|0;
    }

    // Add 37 to element i
    function do_add_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_add(i8a, i, 37);
	return v|0;
    }

    // Subtract 108 from element 20
    function do_sub() {
	var v = 0;
	v = atomic_sub(i8a, 20, 108);
	return v|0;
    }

    // Subtract 108 from element i
    function do_sub_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_sub(i8a, i, 108);
	return v|0;
    }

    // AND 0x33 into element 30
    function do_and() {
	var v = 0;
	v = atomic_and(i8a, 30, 0x33);
	return v|0;
    }

    // AND 0x33 into element i
    function do_and_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_and(i8a, i, 0x33);
	return v|0;
    }

    // OR 0x33 into element 40
    function do_or() {
	var v = 0;
	v = atomic_or(i8a, 40, 0x33);
	return v|0;
    }

    // OR 0x33 into element i
    function do_or_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_or(i8a, i, 0x33);
	return v|0;
    }

    // XOR 0x33 into element 50
    function do_xor() {
	var v = 0;
	v = atomic_xor(i8a, 50, 0x33);
	return v|0;
    }

    // XOR 0x33 into element i
    function do_xor_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_xor(i8a, i, 0x33);
	return v|0;
    }

    // CAS element 100: 0 -> -1
    function do_cas1() {
	var v = 0;
	v = atomic_cmpxchg(i8a, 100, 0, -1);
	return v|0;
    }

    // CAS element 100: -1 -> 0x5A
    function do_cas2() {
	var v = 0;
	v = atomic_cmpxchg(i8a, 100, -1, 0x5A);
	return v|0;
    }

    // CAS element i: 0 -> -1
    function do_cas1_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i8a, i, 0, -1);
	return v|0;
    }

    // CAS element i: -1 -> 0x5A
    function do_cas2_i(i) {
	i = i|0;
	var v = 0;
	v = atomic_cmpxchg(i8a, i, -1, 0x5A);
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

// TODO: halfword arrays
// TODO: signed vs unsigned; negative results

var heap = new SharedArrayBuffer(65536);

////////////////////////////////////////////////////////////
//
// int32 tests

var i32a = new SharedInt32Array(heap);
var i32m = loadModule_int32(this, {}, heap);

var size = 4;

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

assertEq(i32m.cas1_i(size*20000), 0);
assertEq(i32m.cas2_i(size*20000), 0);

assertEq(i32m.or_i(size*20001), 0);
assertEq(i32m.xor_i(size*20001), 0);
assertEq(i32m.and_i(size*20001), 0);
assertEq(i32m.add_i(size*20001), 0);
assertEq(i32m.sub_i(size*20001), 0);

////////////////////////////////////////////////////////////
//
// int8 tests

var i8a = new SharedInt8Array(heap);
var i8m = loadModule_int8(this, {}, heap);

for ( var i=0 ; i < i8a.length ; i++ )
    i8a[i] = 0;

var size = 1;

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

// Out-of-bounds accesses.

assertEq(i8m.cas1_i(80000), 0);
assertEq(i8m.cas2_i(80000), 0);

assertEq(i8m.or_i(80001), 0);
assertEq(i8m.xor_i(80001), 0);
assertEq(i8m.and_i(80001), 0);
assertEq(i8m.add_i(80001), 0);
assertEq(i8m.sub_i(80001), 0);

print("Done");
