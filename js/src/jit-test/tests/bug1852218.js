// |jit-test| skip-if: getBuildConfiguration("arm64-simulator") === true
// This test times out in ARM64 simulator builds.

function makeIonCompiledScript(n) {
  let src = "";
  for (var i = 0; i < n; i++) {
    src += "\n";
  }
  src += "function f() {}";
  eval(src);
  f();
  return f;
}

for (var i = 0; i < 5010; i++) {
  makeIonCompiledScript(i);
}
