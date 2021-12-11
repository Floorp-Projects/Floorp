// |jit-test| skip-if: !('oomTest' in this); --fast-warmup

// Prevent slowness with --ion-eager.
setJitCompilerOption("ion.warmup.trigger", 100);

function f() { return 1; }
function test() {
  oomTest(function() {
    function foo() {
      for (var i = 0; i < 10; i++) {
        f();
        trialInline();
      }
    }
    evaluate(foo.toString() + "foo()");
  });
}
for (var i = 0; i < 3; i++) {
  test();
}
