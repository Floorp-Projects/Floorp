// Appending elements to a dense array should make the array sparse when the
// length exceeds the limit, instead of throwing OOM error.

function test() {
  const MAX_DENSE_ELEMENTS_ALLOCATION = (1 << 28) - 1;
  const VALUES_PER_HEADER = 2;
  const MAX_DENSE_ELEMENTS_COUNT = MAX_DENSE_ELEMENTS_ALLOCATION - VALUES_PER_HEADER;
  const SPARSE_DENSITY_RATIO = 8;
  const MIN_DENSE = MAX_DENSE_ELEMENTS_COUNT / SPARSE_DENSITY_RATIO;
  const MARGIN = 16;

  let a = [];
  // Fill the beginning of array to make it keep dense until length exceeds
  // MAX_DENSE_ELEMENTS_COUNT.
  for (let i = 0; i < MIN_DENSE; i++)
    a[i] = i;

  // Skip from MIN_DENSE to MAX_DENSE_ELEMENTS_COUNT - MARGIN, to reduce the
  // time taken by test.

  // Fill the ending of array to make it sparse at MAX_DENSE_ELEMENTS_COUNT.
  for (let i = MAX_DENSE_ELEMENTS_COUNT - MARGIN; i < MAX_DENSE_ELEMENTS_COUNT + MARGIN; i++)
    a[i] = i;

  // Make sure the last element is defined.
  assertEq(a.length, MAX_DENSE_ELEMENTS_COUNT + MARGIN);
  assertEq(a[a.length - 1], MAX_DENSE_ELEMENTS_COUNT + MARGIN - 1);

  // Make sure elements around MAX_DENSE_ELEMENTS_COUNT are also defined.
  assertEq(a[MAX_DENSE_ELEMENTS_COUNT - 1], MAX_DENSE_ELEMENTS_COUNT - 1);
  assertEq(a[MAX_DENSE_ELEMENTS_COUNT], MAX_DENSE_ELEMENTS_COUNT);
  assertEq(a[MAX_DENSE_ELEMENTS_COUNT + 1], MAX_DENSE_ELEMENTS_COUNT + 1);
}

var config = getBuildConfiguration();
// Takes too long time on debug build.
if (!config.debug) {
  test();
}
