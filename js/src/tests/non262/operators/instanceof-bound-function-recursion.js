function f() {}

var fn = f;
for (var i = 0; i < 100000; ++i) {
  fn = fn.bind();

  // Ensure we don't fallback to @@hasInstance from %FunctionPrototype%.
  Object.defineProperty(fn, Symbol.hasInstance, {
    value: undefined, writable: true, enumerable: true, writable: true
  });

  // Prevent generating overlong names of the form "bound bound bound [...] f".
  Object.defineProperty(fn, "name", {
    value: "", writable: true, enumerable: true, writable: true
  });
}

assertThrowsInstanceOf(
  () => ({}) instanceof fn,
  Error,
  "detect runaway recursion delegating instanceof to bound function target");

reportCompare(0, 0);
