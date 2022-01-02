
gczeal(14);

// The object metadata callback can iterate over the stack. Thus during the
// allocation of the lambda we might inspect the stack which is still incomplete
// because the lambda is not yet reconstructed.
//
// enableShellAllocationMetadataBuilder ignores its argument, because we don't
// permit metadata callbacks to run JS any more, so this test may be
// unnecessary. We'll preserve its structure just in case.
enableShellAllocationMetadataBuilder(function() {});
function f() {
    (function() {
        '' ^ Object
    })();
}
x = 0;
for (var j = 0; j < 99; ++j) {
    x += f();
}

try {
  x = true;
  // Same comment as above.
  enableShellAllocationMetadataBuilder(function([x, y, z], ... Debugger) {});
  for (var i = 0; i < 10; ++i) {
    var f = function() {
      function g() {
	x++;
      }
      g();
    }
    f();
  }
} catch (e) {
  assertEq(e instanceof TypeError, true);
}
