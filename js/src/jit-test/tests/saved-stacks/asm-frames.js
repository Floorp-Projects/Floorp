function AsmModule(stdlib, foreign, heap) {
  "use asm";
  var ffi = foreign.t;

  function doTest() {
    ffi();
  }

  function test() {
    doTest();
  }

  return { test: test };
}

let stack;

function tester() {
  stack = saveStack();
}

const buf = new ArrayBuffer(1024*8);
const module = AsmModule(this, { t: tester }, buf);
module.test();

print(stack);
assertEq(stack.functionDisplayName, "tester");

assertEq(stack.parent.functionDisplayName, "doTest");
assertEq(stack.parent.line, 6);
assertEq(stack.parent.column, 4);

assertEq(stack.parent.parent.functionDisplayName, "test");
assertEq(stack.parent.parent.line, 10);
assertEq(stack.parent.parent.column, 4);

assertEq(stack.parent.parent.parent.line, 24);
assertEq(stack.parent.parent.parent.column, 0);

assertEq(stack.parent.parent.parent.parent, null);
