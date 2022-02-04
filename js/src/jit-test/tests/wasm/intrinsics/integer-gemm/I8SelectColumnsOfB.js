// This file contains all the tests for int8_select_columns_of_b intrinsic. It depends
// on the CommonTestSetup.js script which contains the common functionality
// that is required for testing all the intrinsics.
const COMMON_TEST_SETUP_SCRIPT = "./CommonTestSetup.js"

// All tests for this intrinsic as a string
const ALL_TESTS_AS_STRING =`
let {int8_select_columns_of_b} = instance.exports;

const VALID = {input: 0, rows: ROWS_B_MULTIPLIER, cols: COLUMNS_B_MULTIPLIER, colIndexList: ARRAY_ALIGNMENT << 3, sizeColIndexList: SELECTED_COLUMNS_B_MULTIPLIER, output: ARRAY_ALIGNMENT << 5};

function testInvalidSize() {
  let invalidSize;

  // row: 0
  invalidSize = 0;
  assertErrorMessage(() => int8_select_columns_of_b(VALID.input, invalidSize, VALID.cols, VALID.colIndexList, VALID.sizeColIndexList, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // row: Not an integral multiple of ROWS_B_MULTIPLIER
  invalidSize = ROWS_B_MULTIPLIER + 1;
  assertErrorMessage(() => int8_select_columns_of_b(VALID.input, invalidSize, VALID.cols, VALID.colIndexList, VALID.sizeColIndexList, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // col: 0
  invalidSize = 0;
  assertErrorMessage(() => int8_select_columns_of_b(VALID.input, VALID.rows, invalidSize, VALID.colIndexList, VALID.sizeColIndexList, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // col: Not an integral multiple of COLUMNS_B_MULTIPLIER
  invalidSize = COLUMNS_B_MULTIPLIER + 1;
  assertErrorMessage(() => int8_select_columns_of_b(VALID.input, VALID.rows, invalidSize, VALID.colIndexList, VALID.sizeColIndexList, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // sizeColIndexList: 0
  invalidSize = 0;
  assertErrorMessage(() => int8_select_columns_of_b(VALID.input, VALID.rows, VALID.cols, VALID.colIndexList, invalidSize, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // sizeColIndexList: Not an integral multiple of SELECTED_COLUMNS_B_MULTIPLIER
  invalidSize = SELECTED_COLUMNS_B_MULTIPLIER + 1;
  assertErrorMessage(() => int8_select_columns_of_b(VALID.input, VALID.rows, VALID.cols, VALID.colIndexList, invalidSize, VALID.output), WebAssembly.RuntimeError, /unreachable/);
}

function testInvalidAlignment() {
  let invalidAlignment = ARRAY_ALIGNMENT + 1;

  // input: Not an integral multiple of ARRAY_ALIGNMENT
  assertErrorMessage(() => int8_select_columns_of_b(invalidAlignment, VALID.rows, VALID.cols, VALID.colIndexList, VALID.sizeColIndexList, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);
}

function testOutOfBounds() {
  let outOfBound;

  // input: Out of Bounds
  outOfBound = PageSizeInBytes - ARRAY_ALIGNMENT;
  assertErrorMessage(() => int8_select_columns_of_b(outOfBound, VALID.rows, VALID.cols, VALID.colIndexList, VALID.sizeColIndexList, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);

  // colIndexList: Out of Bounds
  outOfBound = PageSizeInBytes - VALID.sizeColIndexList;
  assertErrorMessage(() => int8_select_columns_of_b(outOfBound, VALID.rows, VALID.cols, outOfBound, VALID.sizeColIndexList, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);

  // output: Out of Bounds
  outOfBound = PageSizeInBytes - (VALID.rows * VALID.sizeColIndexList);
  assertErrorMessage(() => int8_select_columns_of_b(VALID.input, VALID.rows, VALID.cols, VALID.colIndexList, VALID.sizeColIndexList, outOfBound), WebAssembly.RuntimeError, /index out of bounds/);
}

function testSuccessfulCall() {
  // We just test that with valid arguments the intrinsic executes without any error
  int8_select_columns_of_b(VALID.input, VALID.rows, VALID.cols, VALID.colIndexList, VALID.sizeColIndexList, VALID.output);
}

testInvalidSize();
testInvalidAlignment();
testOutOfBounds();
testSuccessfulCall();
`

// Run all the tests
import(COMMON_TEST_SETUP_SCRIPT).then((importedModule) => {
  importedModule.runTest(importedModule.COMMON_TEST_SETUP_AS_STRING + ALL_TESTS_AS_STRING);
});
