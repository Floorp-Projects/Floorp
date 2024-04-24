// Iterate a few times to increase the likelihood for malloc(0) to return a
// non-aligned memory.
for (let i = 0; i < 100; ++i) {
  // Create a zero-length buffer with malloc'ed memory.
  let ab = createExternalArrayBuffer(0);

  // Create a typed array which requires 8-byte alignment.
  let source = new Float64Array(ab);

  // This call shouldn't assert when copying zero bytes from |source|, even when
  // the memory is non-aligned.
  let target = new Float64Array(source);

  // Add some uses of the objects.
  assertEq(ab.byteLength, 0);
  assertEq(source.byteLength, 0);
  assertEq(target.byteLength, 0);
}

// Repeat the above tests with the |createUserArrayBuffer| testing function.
for (let i = 0; i < 100; ++i) {
  let ab = createUserArrayBuffer(0);
  let source = new Float64Array(ab);
  let target = new Float64Array(source);

  assertEq(ab.byteLength, 0);
  assertEq(source.byteLength, 0);
  assertEq(target.byteLength, 0);
}
