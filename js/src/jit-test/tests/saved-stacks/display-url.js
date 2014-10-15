eval(`
  function a() {
    return b();
  }
  //# sourceURL=source-a.js
`);

eval(`
  function b() {
    return c();
  }
  //# sourceURL=source-b.js
`);

eval(`
  function c() {
    return saveStack();
  }
  //# sourceURL=source-c.js
`);

let stack = a();
print(stack);
assertEq(stack.source, "source-c.js");
assertEq(stack.parent.source, "source-b.js");
assertEq(stack.parent.parent.source, "source-a.js");
