const stack = catchAndReturnPendingExceptionStack(function a() {
  (function b() {
    (function c() {
      throw 42;
    }());
  }());
});

assertEq(stack.functionDisplayName, "c");
assertEq(stack.parent.functionDisplayName, "b");
assertEq(stack.parent.parent.functionDisplayName, "a");
assertEq(stack.parent.parent.parent.functionDisplayName, null);
assertEq(stack.parent.parent.parent.parent, null);
