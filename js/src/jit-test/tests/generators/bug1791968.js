function opaqueThrow() { with ({}) {} throw 3; }

function* foo(n) {
  try {
    // Get into the catch block.
    opaqueThrow();
  } catch(v12) {
    // Yield a value.
    yield 1;
  } finally {
    // OSR into Warp with JS_GENERATOR_CLOSING
    // as the pushed exception.
    for (let i = 0; i < 100; i++) { }

    // Create an RPow.
    var x = Math.pow(1,n);

    // When the finally block terminates, we re-throw
    // JS_GENERATOR_CLOSING, and rematerialize the frame
    // for HandleClosingGeneratorReturn, triggering
    // recovery of the RPow.
  }
}

for (let i = 0; i < 30; i++) {
  let gen = foo(1);

  // Advance to the yield in the catch block.
  gen.next();

  // Close the generator.
  gen.return();
}
