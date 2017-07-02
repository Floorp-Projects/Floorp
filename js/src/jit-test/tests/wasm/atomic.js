if (!hasWasmAtomics())
    quit(0);

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
