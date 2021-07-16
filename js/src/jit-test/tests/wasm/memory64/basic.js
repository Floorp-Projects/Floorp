// Basic tests around creating and linking memories with i64 indices

const MaxMemory64Field = 0x1000000000000;
const MaxUint32 = 0xFFFF_FFFF;

// test the validity of different i64 memory types in validation, compilation,
// and the JS-API.
function memoryTypeModuleText(shared, initial, max) {
  return `(module
      (memory i64 ${initial} ${max !== undefined ? max : ''} ${shared ? `shared` : ''})
    )`;
}
function memoryTypeDescriptor(shared, initial, max) {
  return {
    index: 'i64',
    initial,
    maximum: max,
    shared,
  };
}
function validMemoryType(shared, initial, max) {
  wasmValidateText(memoryTypeModuleText(shared, initial, max));
  wasmEvalText(memoryTypeModuleText(shared, initial, max));
  // TODO: JS-API cannot specify pages above UINT32_MAX
  if (max <= MaxUint32) {
    new WebAssembly.Memory(memoryTypeDescriptor(shared, initial, max));
  }
}
function invalidMemoryType(shared, initial, max, compileMessage, jsMessage) {
  wasmFailValidateText(memoryTypeModuleText(shared, initial, max), compileMessage);
  assertErrorMessage(() => wasmEvalText(memoryTypeModuleText(shared, initial, max)), WebAssembly.CompileError, compileMessage);
  // TODO: JS-API cannot specify pages above UINT32_MAX
  if (max === undefined || max <= MaxUint32) {
    assertErrorMessage(() => new WebAssembly.Memory(memoryTypeDescriptor(shared, initial, max)), Error, jsMessage);
  }
}

// valid to define a memory with i64
validMemoryType(false, 0);
// valid to define max with i64
validMemoryType(false, 0, 1);
// invalid for min to be greater than max with i64
invalidMemoryType(false, 2, 1, /minimum must not be greater than maximum/, /bad Memory maximum size/);
// valid to define shared memory with max with i64
validMemoryType(true, 1, 2);
// invalid to define shared memory without max with i64
invalidMemoryType(true, 1, undefined, /maximum length required for shared memory/, /maximum is not specified/);

// test the limits of memory64
validMemoryType(false, 0, MaxMemory64Field);
invalidMemoryType(false, 0, MaxMemory64Field + 1, /maximum memory size too big/, /bad Memory maximum/);
validMemoryType(true, 0, MaxMemory64Field);
invalidMemoryType(true, 0, MaxMemory64Field + 1, /maximum memory size too big/, /bad Memory maximum/);

// test that linking requires index types to be equal
function testLink(importedIndexType, importIndexType) {
  let imported = new WebAssembly.Memory({
    index: importedIndexType,
    initial: 0,
  });
  let testModule = `(module
    (memory (import "" "imported") ${importIndexType} 0)
      )`;
  if (importedIndexType === importIndexType) {
    wasmEvalText(testModule, {"": {imported}});
  } else {
    assertErrorMessage(() => wasmEvalText(testModule, {"": {imported}}), WebAssembly.LinkError, /index type/);
  }
}
testLink('i64', 'i64');
testLink('i32', 'i32');
testLink('i64', 'i32');
testLink('i32', 'i64');
