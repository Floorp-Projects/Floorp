let err;
const stack = catchAndReturnPendingExceptionStack(function a() {
  err = new Error();
  (function b() {
    (function c() {
      throw err;
    }());
  }());
});

assertEq(!!err, true);

// Yes, they're different!
assertEq(stack.toString() != err.stack, true);

assertEq(stack.functionDisplayName, "c");
assertEq(stack.parent.functionDisplayName, "b");
assertEq(stack.parent.parent.functionDisplayName, "a");
assertEq(stack.parent.parent.parent.functionDisplayName, null);
assertEq(stack.parent.parent.parent.parent, null);
