// Tests of limits of memory and table types

const PageSize = PageSizeInBytes;
const MemoryMaxValid = 65536;
const MemoryMaxRuntime = MaxPagesIn32BitMemory;

const TableMaxValid = 0xffff_ffff;
const TableMaxRuntime = 10_000_000;

// Test that a memory type is valid within a module
function testMemoryValidate(initial, maximum, shared) {
  wasmValidateText(`(module
    (memory ${initial} ${maximum || ''} ${shared ? 'shared' : ''})
  )`);
}

testMemoryValidate(0, undefined, false);
testMemoryValidate(1, undefined, false);
testMemoryValidate(0, 1, false);
testMemoryValidate(0, 1, true);
testMemoryValidate(1, 1, false);
testMemoryValidate(1, 1, true);
testMemoryValidate(MemoryMaxValid, undefined, false);
testMemoryValidate(MemoryMaxValid, MemoryMaxValid, false);
testMemoryValidate(MemoryMaxValid, MemoryMaxValid, true);

// Test that a memory type is not valid within a module
function testMemoryFailValidate(initial, maximum, shared, pattern) {
  wasmFailValidateText(`(module
    (memory ${initial} ${maximum || ''} ${shared ? 'shared' : ''})
  )`, pattern);
}

testMemoryFailValidate(2, 1, false, /size minimum must not be greater than maximum/);
testMemoryFailValidate(1, undefined, true, /maximum length required for shared memory/);
testMemoryFailValidate(MemoryMaxValid + 1, undefined, false, /initial memory size too big/);
testMemoryFailValidate(MemoryMaxValid, MemoryMaxValid + 1, false, /maximum memory size too big/);
testMemoryFailValidate(MemoryMaxValid, MemoryMaxValid + 1, true, /maximum memory size too big/);

// Test that a memory type is invalid for constructing a WebAssembly.Memory
function testMemoryFailConstruct(initial, maximum, shared, pattern) {
  assertErrorMessage(() => new WebAssembly.Memory({
    initial,
    maximum,
    shared
  }), RangeError, pattern);
}

// Test initial length, giving a maximum only if required due to being shared
testMemoryFailConstruct(MemoryMaxValid + 1, undefined, false, /bad Memory initial size/);
testMemoryFailConstruct(MemoryMaxValid + 1, MemoryMaxValid + 1, true, /bad Memory initial size/);
// Test maximum length
testMemoryFailConstruct(0, MemoryMaxValid + 1, false, /bad Memory maximum size/);
testMemoryFailConstruct(0, MemoryMaxValid + 1, true, /bad Memory maximum size/);

// Test that a memory type can be instantiated within a module or constructed
// with a WebAssembly.Memory
function testMemoryCreate(initial, maximum, shared) {
  // May OOM, but must not fail to validate
  try {
      wasmEvalText(`(module
        (memory ${initial} ${maximum || ''} ${shared ? 'shared' : ''})
      )`);
  } catch (e) {
      assertEq(String(e).indexOf("out of memory") !== -1, true, `${e}`);
  }
  try {
    new WebAssembly.Memory({initial, maximum, shared});
  } catch (e) {
    assertEq(String(e).indexOf("out of memory") !== -1, true, `${e}`);
  }
}

testMemoryCreate(0, undefined, false);
testMemoryCreate(1, undefined, false);
testMemoryCreate(0, 1, false);
testMemoryCreate(0, 1, true);
testMemoryCreate(1, 1, false);
testMemoryCreate(1, 1, true);
testMemoryCreate(MemoryMaxRuntime, undefined, false);
testMemoryCreate(MemoryMaxRuntime, MemoryMaxValid, false);
testMemoryCreate(MemoryMaxRuntime, MemoryMaxValid, true);

// Test that a memory type cannot be instantiated within a module or constructed
// with a WebAssembly.Memory

if (MemoryMaxRuntime < 65536) {
    let testMemoryFailCreate = function(initial, maximum, shared) {
        assertErrorMessage(() => wasmEvalText(`(module
    (memory ${initial} ${maximum || ''} ${shared ? 'shared' : ''})
  )`), WebAssembly.RuntimeError, /too many memory pages/);
        assertErrorMessage(() => new WebAssembly.Memory({
            initial,
            maximum,
            shared
        }), WebAssembly.RuntimeError, /too many memory pages/);
    }

    testMemoryFailCreate(MemoryMaxRuntime + 1, undefined, false);
    testMemoryFailCreate(MemoryMaxRuntime + 1, MemoryMaxValid, false);
    testMemoryFailCreate(MemoryMaxRuntime + 1, MemoryMaxValid, true);
} else {
    let testMemoryFailCreate = function(initial, maximum, shared) {
        assertErrorMessage(() => wasmEvalText(`(module
    (memory ${initial} ${maximum || ''} ${shared ? 'shared' : ''})
  )`), WebAssembly.CompileError, /(initial memory size too big)|(memory size minimum must not be greater than maximum)/);
        assertErrorMessage(() => new WebAssembly.Memory({
            initial,
            maximum,
            shared
        }), RangeError, /bad Memory initial size/);
    }

    testMemoryFailCreate(MemoryMaxRuntime + 1, undefined, false);
    testMemoryFailCreate(MemoryMaxRuntime + 1, MemoryMaxValid, false);
    testMemoryFailCreate(MemoryMaxRuntime + 1, MemoryMaxValid, true);
}


// Test that a memory type cannot be grown from initial to a target due to an
// implementation limit
function testMemoryFailGrow(initial, maximum, target, shared) {
  let {run} = wasmEvalText(`(module
    (memory ${initial} ${maximum || ''} ${shared ? 'shared' : ''})
    (func (export "run") (result i32)
      i32.const ${target - initial}
      memory.grow
    )
  )`).exports;
  assertEq(run(), -1, 'failed to grow');

  let mem = new WebAssembly.Memory({
    initial,
    maximum,
    shared
  });
  assertErrorMessage(() => mem.grow(target - initial), RangeError, /failed to grow memory/);
}

testMemoryFailGrow(1, undefined, MemoryMaxRuntime + 1, false);
testMemoryFailGrow(1, MemoryMaxValid, MemoryMaxRuntime + 1, false);
testMemoryFailGrow(1, MemoryMaxValid, MemoryMaxRuntime + 1, true);

// Test that a table type is valid within a module
function testTableValidate(initial, maximum) {
  wasmValidateText(`(module
    (table ${initial} ${maximum || ''} anyfunc)
  )`);
}

testTableValidate(0, undefined);
testTableValidate(1, undefined);
testTableValidate(0, 1);
testTableValidate(1, 1);
testTableValidate(TableMaxValid, undefined);
testTableValidate(TableMaxValid, TableMaxValid);

// Test that a table type is not valid within a module
function testTableFailValidate(initial, maximum, pattern) {
  wasmFailValidateText(`(module
    (table ${initial} ${maximum || ''} anyfunc)
  )`, pattern);
}

testTableFailValidate(2, 1, /size minimum must not be greater than maximum/);
// The maximum valid table value is equivalent to the maximum encodable limit
// value, so we cannot test too large of a table limit in a module.
assertEq(TableMaxValid + 1 > 0xffffffff, true);

// Test that a table type is invalid for constructing a WebAssembly.Table
function testTableFailConstruct(initial, maximum, pattern) {
  assertErrorMessage(() => new WebAssembly.Table({
    initial,
    maximum,
    element: 'anyfunc',
  }), TypeError, pattern);
}

testTableFailConstruct(TableMaxValid + 1, undefined, /bad Table initial size/);
testTableFailConstruct(0, TableMaxValid + 1, /bad Table maximum size/);

// Test that a table type can be instantiated within a module or constructed
// with a WebAssembly.Table
function testTableCreate(initial, maximum) {
  // May OOM, but must not fail to validate
  try {
      wasmEvalText(`(module
        (table ${initial} ${maximum || ''} anyfunc)
      )`);
  } catch (e) {
      assertEq(String(e).indexOf("out of memory") !== -1, true, `${e}`);
  }
  try {
    new WebAssembly.Table({
      initial,
      maximum,
      element: 'anyfunc',
    });
  } catch (e) {
    assertEq(String(e).indexOf("out of memory") !== -1, true, `${e}`);
  }
}

testTableCreate(0, undefined);
testTableCreate(1, undefined);
testTableCreate(0, 1);
testTableCreate(1, 1);
testTableCreate(TableMaxRuntime, undefined);
testTableCreate(TableMaxRuntime, TableMaxValid);

// Test that a table type cannot be instantiated within a module or constructed
// with a WebAssembly.Table
function testTableFailCreate(initial, maximum, pattern) {
  assertErrorMessage(() => wasmEvalText(`(module
    (table ${initial} ${maximum || ''} anyfunc)
  )`), WebAssembly.RuntimeError, pattern);
  assertErrorMessage(() => new WebAssembly.Table({
    initial,
    maximum,
    element: 'anyfunc',
  }), WebAssembly.RuntimeError, pattern);
}

testTableFailCreate(TableMaxRuntime + 1, undefined, /too many table elements/);
testTableFailCreate(TableMaxRuntime + 1, TableMaxValid, /too many table elements/);

// Test that a table type cannot be grown from initial to a target due to an
// implementation limit
function testTableFailGrow(initial, maximum, target) {
  let {run} = wasmEvalText(`(module
    (table ${initial} ${maximum || ''} externref)
    (func (export "run") (result i32)
      ref.null extern
      i32.const ${target - initial}
      table.grow
    )
  )`).exports;
  assertEq(run(), -1, 'failed to grow');

  let tab = new WebAssembly.Table({
    initial,
    maximum,
    element: 'externref',
  });
  assertErrorMessage(() => tab.grow(target - initial), RangeError, /failed to grow table/);
}

testTableFailGrow(1, undefined, TableMaxRuntime + 1);
testTableFailGrow(1, TableMaxValid, TableMaxRuntime + 1);
