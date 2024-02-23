// |jit-test| allow-oom; allow-unhandlable-oom
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
