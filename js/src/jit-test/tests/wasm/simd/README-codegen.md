About whitebox code generation tests (*-codegen.js):

These test that extraneous moves are not inserted by the register
allocator or code generator under ideal conditions: when it is in
principle possible for the code generator and register allocator to
use inputs where they are and generate outputs directly in the target
registers.

These tests are both limited in scope and brittle in the face of
changes to the register allocator, but how else would we test that
code generation and register allocation work when presented with the
easiest case?  And if they don't work then, when will they work?

For a reliable test, the inputs must be known to be in the fixed
parameter registers and the result must be known to be desired in the
fixed function result register.

Sometimes, to test optimal codegen, we need the inputs to be in
reversed or permuted locations so as to avoid generating moves that
are inserted by the regalloc to adapt to the function signature.

In the test cases, the expected output is expressed as a multi-line
regular expression.  The first line of each expected output is the
tail end of the prologue; subsequent lines comprise the operation;
finally there is the beginning of the epilogue.  Sometimes there is
only the end of the prologue and the beginning of the operation, as
the operation is long and we don't care about its tail.

