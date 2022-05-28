// Make sure we use an IC call.
setJitCompilerOption("ion.forceinlineCaches", 1);

// Assume |eval| is always a native function.
var obj = Object.defineProperty({}, "prop", {
  set: eval
});

var p;
for (let i = 0; i < 1000; ++i) {
  // Call the native setter (eval).
  obj.prop = `p = ${i}`;

  assertEq(p, i);
}
