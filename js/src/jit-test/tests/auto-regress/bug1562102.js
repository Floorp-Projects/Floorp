// |jit-test| allow-oom; allow-unhandlable-oom; skip-if: !('oomTest' in this)
oomTest(
  function() {
    evaluate(`
class C {
  c;
}
`, {
  compileAndGo: true
});
  }
)
