var depth = 0;
test();
function test() {
  // |test()| is called recursively. When the generator runs in the JIT, the
  // recursion limit is ~20x higher than in the interpreter. Limit the depth
  // here so that the test doesn't timeout or becomes extremely slow.
  if (++depth > 400)
     return;

  var catch1, catch2, catch3, finally1, finally2, finally3;
  function gen() {
    yield 1;
    try {
      try {
        try {
          yield 1;
        } finally {
          test();
        }
      } catch (e) {
        finally2 = true;
      }
    } catch (e) {    }
  }
  iter = gen();
  iter.next();
  iter.next();
  iter.close();
  gc();
} 
