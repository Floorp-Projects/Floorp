// |jit-test| exitstatus: 3; skip-if: !('oomAfterAllocations' in this)

oomAfterAllocations(50, 2);
eval("(function() {'use asm'; function f() { return +pow(.0, .0) })")
