// Test that constructors that abort due to asm.js do not assert due to the
// parser keeping track of the FunctionBox corresponding to the constructor.

class a {
  constructor() {
    "use asm";
  }
}
