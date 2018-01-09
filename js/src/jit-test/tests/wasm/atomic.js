if (!wasmThreadsSupported())
    quit(0);

const oob = /index out of bounds/;
const unaligned = /unaligned memory access/;
const RuntimeError = WebAssembly.RuntimeError;

// Check that the output of wasmTextToBinary verifies correctly.

let SHARED = 'shared';
let UNSHARED = '';
let sharedError = 'memory with atomic operations without shared memory';

for (let [type,view] of [['i32','8_u'],['i32','16_u'],['i32',''],['i64','8_u'],['i64','16_u'],['i64','32_u'],['i64','']]) {
    {
	let text = (shared) => `(module (memory 1 1 ${shared})
				 (func (result ${type}) (${type}.atomic.load${view} (i32.const 0)))
				 (export "" 0))`;
	assertEq(valText(text(SHARED)), true);
	assertEq(valText(text(UNSHARED)), false);
    }

    {
	let text = (shared) => `(module (memory 1 1 ${shared})
				 (func (${type}.atomic.store${view} (i32.const 0) (${type}.const 1)))
				 (export "" 0))`;
	assertEq(valText(text(SHARED)), true);
	assertEq(valText(text(UNSHARED)), false);
    }

    {
	let text = (shared) => `(module (memory 1 1 ${shared})
				 (func (result ${type})
				  (${type}.atomic.rmw${view}.cmpxchg (i32.const 0) (${type}.const 1) (${type}.const 2)))
				 (export "" 0))`;
	assertEq(valText(text(SHARED)), true);
	assertEq(valText(text(UNSHARED)), false);
    }

    for (let op of ['add','and','or','sub','xor','xchg']) {
	// Operate with appropriately-typed value 1 on address 0
	let text = (shared) => `(module (memory 1 1 ${shared})
				 (func (result ${type}) (${type}.atomic.rmw${view}.${op} (i32.const 0) (${type}.const 1)))
				 (export "" 0))`;

	assertEq(valText(text(SHARED)), true);
	assertEq(valText(text(UNSHARED)), false);
    }
}

for (let type of ['i32', 'i64']) {
    let text = (shared) => `(module (memory 1 1 ${shared})
			     (func (result i32) (${type}.atomic.wait (i32.const 0) (${type}.const 1) (i64.const -1)))
			     (export "" 0))`;
    assertEq(valText(text(SHARED)), true);
    assertEq(valText(text(UNSHARED)), false);
}

{
    let text = (shared) => `(module (memory 1 1 ${shared})
			     (func (result i32) (atomic.wake (i32.const 0) (i32.const 1)))
			     (export "" 0))`;
    assertEq(valText(text(SHARED)), true);
    assertEq(valText(text(UNSHARED)), false);
}

// Required explicit alignment for WAIT is the size of the datum

for (let [type,align,good] of [['i32',1,false],['i32',2,false],['i32',4,true],['i32',8,false],
			       ['i64',1,false],['i64',2,false],['i64',4,false],['i64',8,true]])
{
    let text = `(module (memory 1 1 shared)
		 (func (result i32) (${type}.atomic.wait align=${align} (i32.const 0) (${type}.const 1) (i64.const -1)))
		 (export "" 0))`;
    assertEq(valText(text), good);
}

// Required explicit alignment for WAKE is 4

for (let align of [1, 2, 4, 8]) {
    let text = `(module (memory 1 1 shared)
		 (func (result i32) (atomic.wake align=${align} (i32.const 0) (i32.const 1)))
		 (export "" 0))`;
    assertEq(valText(text), align == 4);
}

// Check that the text output is sane.

for (let [type,view] of [['i32','8_u'],['i32','16_u'],['i32',''],['i64','8_u'],['i64','16_u'],['i64','32_u'],['i64','']]) {
    let addr = "i32.const 48";
    let value = `${type}.const 1`;
    let value2 = `${type}.const 2`;
    for (let op of ["add", "and", "or", "xor", "xchg"]) {
	let operator = `${type}.atomic.rmw${view}.${op}`;
	let text = `(module (memory 1 1 shared)
		     (func (result ${type}) (${operator} (${addr}) (${value})))
		     (export "" 0))`;
	checkRoundTrip(text, [addr, value, operator]);
    }
    {
	let operator = `${type}.atomic.rmw${view}.cmpxchg`;
	let text = `(module (memory 1 1 shared)
		     (func (result ${type}) (${operator} (${addr}) (${value}) (${value2})))
		     (export "" 0))`;
	checkRoundTrip(text, [addr, value, value2, operator]);
    }
    {
	let operator = `${type}.atomic.load${view}`;
	let text = `(module (memory 1 1 shared)
		     (func (result ${type}) (${operator} (${addr})))
		     (export "" 0))`;
	checkRoundTrip(text, [addr, operator]);
    }
    {
	let operator = `${type}.atomic.store${view}`;
	let text = `(module (memory 1 1 shared)
		     (func (${operator} (${addr}) (${value})))
		     (export "" 0))`;
	checkRoundTrip(text, [addr, value, operator]);
    }
}

for (let type of ['i32', 'i64']) {
    let addr = "i32.const 48";
    let operator = `${type}.atomic.wait`
    let value = `${type}.const 1`;
    let timeout = "i64.const 314159";
    let text = `(module (memory 1 1 shared)
		 (func (result i32) (${operator} (${addr}) (${value}) (${timeout})))
		 (export "" 0))`;
    checkRoundTrip(text, [addr, value, timeout, operator]);
}

{
    let addr = "i32.const 48";
    let operator = "atomic.wake"
    let count = "i32.const 1";
    let text = `(module (memory 1 1 shared)
		 (func (result i32) (${operator} (${addr}) (${count})))
		 (export "" 0))`;
    checkRoundTrip(text, [addr, count, operator]);
}

function valText(text) {
    return WebAssembly.validate(wasmTextToBinary(text));
}

function checkRoundTrip(text, ss) {
    let input = wasmTextToBinary(text);
    let output = wasmBinaryToText(input).split("\n").map(String.trim);
    for (let s of output) {
	if (ss.length == 0)
	    break;
	if (s.match(ss[0]))
	    ss.shift();
    }
    assertEq(ss.length, 0);
}

// Test that atomic operations work.

function I64(hi, lo) {
    this.high = hi;
    this.low = lo;
}
I64.prototype.toString = function () {
    return "(" + this.high.toString(16) + " " + this.low.toString(16) + ")";
}

function Uint64Array(arg) {
    let buffer = arg;
    if (typeof arg == "number")
	buffer = new ArrayBuffer(arg*8);
    this.buf = buffer;
    this.elem = new Uint32Array(buffer);
}

Uint64Array.BYTES_PER_ELEMENT = 8;

Uint8Array.prototype.read = function (n) { return this[n] }
Uint16Array.prototype.read = function (n) { return this[n] }
Uint32Array.prototype.read = function (n) { return this[n] }
Uint64Array.prototype.read = function (n) {
    return new I64(this.elem[n*2+1], this.elem[n*2]);
}

Uint8Array.prototype.write = function (n,v) { this[n] = v }
Uint16Array.prototype.write = function (n,v) { this[n] = v }
Uint32Array.prototype.write = function (n,v) { this[n] = v}
Uint64Array.prototype.write = function (n,v) {
    if (typeof v == "number") {
	// Note, this chops v if v is too large
	this.elem[n*2] = v;
	this.elem[n*2+1] = 0;
    } else {
	this.elem[n*2] = v.low;
	this.elem[n*2+1] = v.high;
    }
}

// Widen a one-byte value to a k-byte value where k is TA's width.
// Complementation leads to better error checking, probably.

function widen(TA, value, complement = true) {
    let n = value;
    let s = "";
    for ( let i=0; i < Math.min(TA.BYTES_PER_ELEMENT, 4); i++ ) {
	let v = (256 + n).toString(16);
	s = s + v.substring(v.length-2);
	if (complement)
	    n = ~n;
    }
    if (TA.BYTES_PER_ELEMENT == 8)
	s = s + s;
    s = "0x" + s;

    n = value;
    let num = 0;
    for ( let i=0; i < Math.min(TA.BYTES_PER_ELEMENT, 4); i++ ) {
	num = (num << 8) | (n & 255);
	if (complement)
	    n = ~n;
    }
    num = num >>> 0;

    if (TA.BYTES_PER_ELEMENT == 8) {
	return [s, new I64(num, num)];
    } else {
	return [s, num];
    }
}

// Atomic RMW ops are sometimes used for effect, sometimes for their value, and
// in SpiderMonkey code generation differs for the two cases, so we need to test
// both.  Also, there may be different paths for constant addresses/operands and
// variable ditto, so test as many combinations as possible.

var RMWOperation =
{
    loadStoreModule(type, view, address, operand) {
	let bin = wasmTextToBinary(
	    `(module
	      (memory (import "" "memory") 1 1 shared)
	      (func (export "st") (param i32)
	       (${type}.atomic.store${view} ${address} ${operand}))
	      (func $ld (param i32) (result ${type})
	       (${type}.atomic.load${view} ${address}))
	      (func (export "ld") (param i32) (result i32)
	       (${type}.eq (call $ld (get_local 0)) ${operand})))`);
	let mod = new WebAssembly.Module(bin);
	let mem = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
	let ins = new WebAssembly.Instance(mod, {"": {memory: mem}});
	return [mem, ins.exports.ld, ins.exports.st];
    },

    opModuleEffect(type, view, address, op, operand, ignored) {
	let bin = wasmTextToBinary(
	    `(module
	      (memory (import "" "memory") 1 1 shared)
	      (func (export "f") (param i32) (result i32)
	       (drop (${type}.atomic.rmw${view}.${op} ${address} ${operand}))
	       (i32.const 1)))`);
	let mod = new WebAssembly.Module(bin);
	let mem = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
	let ins = new WebAssembly.Instance(mod, {"": {memory: mem}});
	return [mem, ins.exports.f];
    },

    opModuleReturned(type, view, address, op, operand, expected) {
	let bin = wasmTextToBinary(
	    `(module
	      (memory (import "" "memory") 1 1 shared)
	      (func $_f (param i32) (result ${type})
	       (${type}.atomic.rmw${view}.${op} ${address} ${operand}))
	      (func (export "f") (param i32) (result i32)
	       (${type}.eq (call $_f (get_local 0)) (${type}.const ${expected}))))`);
	let mod = new WebAssembly.Module(bin);
	let mem = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
	let ins = new WebAssembly.Instance(mod, {"": {memory: mem}});
	return [mem, ins.exports.f];
    },

    cmpxchgModuleEffect(type, view, address, operand1, operand2, ignored) {
	let bin = wasmTextToBinary(
	    `(module
	      (memory (import "" "memory") 1 1 shared)
	      (func (export "f") (param i32)
	       (drop (${type}.atomic.rmw${view}.cmpxchg ${address} ${operand1} ${operand2}))))`);
	let mod = new WebAssembly.Module(bin);
	let mem = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
	let ins = new WebAssembly.Instance(mod, {"": {memory: mem}});
	return [mem, ins.exports.f];
    },

    cmpxchgModuleReturned(type, view, address, operand1, operand2, expected) {
	let bin = wasmTextToBinary(
	    `(module
	      (memory (import "" "memory") 1 1 shared)
	      (func $_f (param i32) (result ${type})
	       (${type}.atomic.rmw${view}.cmpxchg ${address} ${operand1} ${operand2}))
	      (func (export "f") (param i32) (result i32)
	       (${type}.eq (call $_f (get_local 0)) (${type}.const ${expected}))))`);
	let mod = new WebAssembly.Module(bin);
	let mem = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
	let ins = new WebAssembly.Instance(mod, {"": {memory: mem}});
	return [mem, ins.exports.f];
    },

    assertZero(array, LOC) {
	for ( let i=0 ; i < 100 ; i++ ) {
	    if (i != LOC)
		assertNum(array.read(i), 0);
	}
    },

    run() {
	const LOC = 13;		// The cell we operate on
	const OPD1 = 37;	// Sometimes we'll put an operand here
	const OPD2 = 42;	// Sometimes we'll put another operand here

	for ( let [type, variations] of
	          [["i32", [[Uint8Array,"8_u"], [Uint16Array,"16_u"], [Uint32Array,""]]],
		   ["i64", [[Uint8Array,"8_u"], [Uint16Array,"16_u"], [Uint32Array,"32_u"], [Uint64Array,""]]]] )
	{
	    for ( let [TA, view] of variations )
	    {
		for ( let addr of [`(i32.const ${LOC * TA.BYTES_PER_ELEMENT})`,
				   `(get_local 0)`] )
		{
		    for ( let [initial, operand] of [[0x12, 0x37]] )
		    {
			let [opd_str, opd_num] = widen(TA, operand);
			for ( let rhs of [`(${type}.const ${opd_str})`,
					  `(${type}.load${view} (i32.const ${OPD1 * TA.BYTES_PER_ELEMENT}))`] )
			{
			    let [mem, ld, st] = this.loadStoreModule(type, view, addr, rhs);
			    let array = new TA(mem.buffer);
			    array.write(OPD1, opd_num);
			    array.write(LOC, initial);
			    st(LOC * TA.BYTES_PER_ELEMENT);
			    let res = ld(LOC * TA.BYTES_PER_ELEMENT);
			    assertEq(res, 1);
			    assertNum(array.read(LOC), opd_num);
			    array.write(OPD1, 0);
			    this.assertZero(array, LOC);
			}
		    }

		    for ( let [op, initial, operand, expected] of [["add", 37, 5, 42],
								   ["sub", 42, 5, 37],
								   ["and", 0x45, 0x13, 0x01],
								   ["or",  0x45, 0x13, 0x57],
								   ["xor", 0x45, 0x13, 0x56],
								   ["xchg", 0x45, 0x13, 0x13]] )
		    {
			let complement = op == "xchg";
			let [ini_str, ini_num] = widen(TA, initial, complement);
			let [opd_str, opd_num] = widen(TA, operand, complement);
			let [exp_str, exp_num] = widen(TA, expected, complement);
			for ( let rhs of [`(${type}.const ${opd_str})`,
					  `(${type}.load${view} (i32.const ${OPD1 * TA.BYTES_PER_ELEMENT}))`] )
			{
			    for ( let [generateIt, checkIt] of [["opModuleEffect", false], ["opModuleReturned", true]] )
			    {
				let [mem, f] = this[generateIt](type, view, addr, op, rhs, ini_str);
				let array = new TA(mem.buffer);
				array.write(OPD1, opd_num);
				array.write(LOC, ini_num);
				let res = f(LOC * TA.BYTES_PER_ELEMENT);
				if (checkIt)
				    assertEq(res, 1);
				assertNum(array.read(LOC), exp_num);
				array.write(OPD1, 0);
				this.assertZero(array, LOC);
			    }
			}
		    }

		    for ( let [initial, operand1, operand2, expected] of [[33, 33, 44, 44], [33, 44, 55, 33]] )
		    {
			let [ini_str, ini_num] = widen(TA, initial);
			let [opd1_str, opd1_num] = widen(TA, operand1);
			let [opd2_str, opd2_num] = widen(TA, operand2);
			let [exp_str, exp_num] = widen(TA, expected);
			for ( let op1 of [`(${type}.const ${opd1_str})`,
					  `(${type}.load${view} (i32.const ${OPD1 * TA.BYTES_PER_ELEMENT}))`] )
			{
			    for ( let op2 of [`(${type}.const ${opd2_str})`,
					      `(${type}.load${view} (i32.const ${OPD2 * TA.BYTES_PER_ELEMENT}))`] )
			    {
				for ( let [generateIt, checkIt] of [["cmpxchgModuleEffect", false], ["cmpxchgModuleReturned", true]] )
				{
				    let [mem, f] = this[generateIt](type, view, addr, op1, op2, ini_str);
				    let array = new TA(mem.buffer);
				    array.write(OPD1, opd1_num);
				    array.write(OPD2, opd2_num);
				    array.write(LOC, ini_num);
				    let res = f(LOC * TA.BYTES_PER_ELEMENT);
				    if (checkIt)
					assertEq(res, 1);
				    assertNum(array.read(13), exp_num);
				    array.write(OPD1, 0);
				    array.write(OPD2, 0);
				    this.assertZero(array, LOC);
				}
			    }
			}
		    }
		}
	    }
	}
    }
};

RMWOperation.run();

function assertNum(a, b) {
    if (typeof a == "number" && typeof b == "number")
	assertEq(a, b);
    else if (typeof a == "number") {
	assertEq(a, b.low);
	assertEq(0, b.high);
    } else if (typeof b == "number") {
	assertEq(a.low, b);
	assertEq(a.high, 0);
    } else {
	assertEq(a.high, b.high);
	assertEq(a.low, b.low);
    }
}

// Test bounds and alignment checking on atomic ops

var BoundsAndAlignment =
{
    loadModule(type, ext, offset) {
	return wasmEvalText(
	    `(module
	      (memory 1 1 shared)
	      (func $0 (param i32) (result ${type})
	       (${type}.atomic.load${ext} offset=${offset} (get_local 0)))
	      (func (export "f") (param i32)
	       (drop (call $0 (get_local 0)))))
	    `).exports.f;
    },

    loadModuleIgnored(type, ext, offset) {
	return wasmEvalText(
	    `(module
	      (memory 1 1 shared)
	      (func (export "f") (param i32)
	       (drop (${type}.atomic.load${ext} offset=${offset} (get_local 0)))))
	    `).exports.f;
    },

    storeModule(type, ext, offset) {
	return wasmEvalText(
	    `(module
	      (memory 1 1 shared)
	      (func (export "f") (param i32)
	       (${type}.atomic.store${ext} offset=${offset} (get_local 0) (${type}.const 37))))
	    `).exports.f;
    },

    opModule(type, ext, offset, op) {
	return wasmEvalText(
	    `(module
	      (memory 1 1 shared)
	      (func $0 (param i32) (result ${type})
	       (${type}.atomic.rmw${ext}.${op} offset=${offset} (get_local 0) (${type}.const 37)))
	      (func (export "f") (param i32)
	       (drop (call $0 (get_local 0)))))
	    `).exports.f;
    },

    opModuleForEffect(type, ext, offset, op) {
	return wasmEvalText(
	    `(module
	      (memory 1 1 shared)
	      (func (export "f") (param i32)
	       (drop (${type}.atomic.rmw${ext}.${op} offset=${offset} (get_local 0) (${type}.const 37)))))
	    `).exports.f;
    },

    cmpxchgModule(type, ext, offset) {
	return wasmEvalText(
	    `(module
	      (memory 1 1 shared)
	      (func $0 (param i32) (result ${type})
	       (${type}.atomic.rmw${ext}.cmpxchg offset=${offset} (get_local 0) (${type}.const 37) (${type}.const 42)))
	      (func (export "f") (param i32)
	       (drop (call $0 (get_local 0)))))
	    `).exports.f;
    },

    run() {
	for ( let [type, variations] of [["i32", [["8_u", 1], ["16_u", 2], ["", 4]]],
					 ["i64", [["8_u",1], ["16_u",2], ["32_u",4], ["",8]]]] )
	{
	    for ( let [ext,size] of variations )
	    {
		// Aligned but out-of-bounds
		let addrs = [[65536, 0, oob], [65536*2, 0, oob], [65532, 4, oob],
			     [65533, 3, oob], [65534, 2, oob], [65535, 1, oob]];

		// In-bounds but unaligned
		for ( let i=1 ; i < size ; i++ )
		    addrs.push([65520, i, unaligned]);

		// Both out-of-bounds and unaligned.  The spec leaves it unspecified
		// whether we see the OOB message or the unaligned message (they are
		// both "traps").  In Firefox, the unaligned check comes first.
		for ( let i=1 ; i < size ; i++ )
		    addrs.push([65536, i, unaligned]);

		for ( let [ base, offset, re ] of addrs )
		{
		    assertErrorMessage(() => this.loadModule(type, ext, offset)(base), RuntimeError, re);
		    assertErrorMessage(() => this.loadModuleIgnored(type, ext, offset)(base), RuntimeError, re);
		    assertErrorMessage(() => this.storeModule(type, ext, offset)(base), RuntimeError, re);
		    for ( let op of [ "add", "sub", "and", "or", "xor", "xchg" ]) {
			assertErrorMessage(() => this.opModule(type, ext, offset, op)(base), RuntimeError, re);
			assertErrorMessage(() => this.opModuleForEffect(type, ext, offset, op)(base), RuntimeError, re);
		    }
		    assertErrorMessage(() => this.cmpxchgModule(type, ext, offset)(base), RuntimeError, re);
		}
	    }
	}
    }
}

BoundsAndAlignment.run();

// Bounds and alignment checks on wait and wake

assertErrorMessage(() => wasmEvalText(`(module (memory 1 1 shared)
					(func (param i32) (result i32)
					 (i32.atomic.wait (get_local 0) (i32.const 1) (i64.const -1)))
					(export "" 0))`).exports[""](65536),
		   RuntimeError, oob);

assertErrorMessage(() => wasmEvalText(`(module (memory 1 1 shared)
					(func (param i32) (result i32)
					 (i64.atomic.wait (get_local 0) (i64.const 1) (i64.const -1)))
					(export "" 0))`).exports[""](65536),
		   RuntimeError, oob);

assertErrorMessage(() => wasmEvalText(`(module (memory 1 1 shared)
					(func (param i32) (result i32)
					 (i32.atomic.wait (get_local 0) (i32.const 1) (i64.const -1)))
					(export "" 0))`).exports[""](65501),
		   RuntimeError, unaligned);

assertErrorMessage(() => wasmEvalText(`(module (memory 1 1 shared)
					(func (param i32) (result i32)
					 (i64.atomic.wait (get_local 0) (i64.const 1) (i64.const -1)))
					(export "" 0))`).exports[""](65501),
		   RuntimeError, unaligned);

assertErrorMessage(() => wasmEvalText(`(module (memory 1 1 shared)
					(func (param i32) (result i32)
					 (atomic.wake (get_local 0) (i32.const 1)))
					(export "" 0))`).exports[""](65536),
		   RuntimeError, oob);

// Minimum run-time alignment for WAKE is 4
for (let addr of [1,2,3,5,6,7]) {
    assertErrorMessage(() => wasmEvalText(`(module (memory 1 1 shared)
					    (func (export "f") (param i32) (result i32)
					     (atomic.wake (get_local 0) (i32.const 1))))`).exports.f(addr),
		       RuntimeError, unaligned);
}

// Ensure alias analysis works even if atomic and non-atomic accesses are
// mixed.
assertErrorMessage(() => wasmEvalText(`(module
  (memory 0 1 shared)
  (func (export "main")
    i32.const 1
    i32.const 2816
    i32.atomic.rmw16_u.xchg align=2
    i32.load16_s offset=83 align=1
    drop
  )
)`).exports.main(), RuntimeError, unaligned);
