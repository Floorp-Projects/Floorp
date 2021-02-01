// |jit-test| error: Error; --cpu-count=2; --fast-warmup

function main() {
  new main;
}
try { main(); } catch(exc) {}
evaluate(`
  function main() {
    function v0(v1,v2) {
      try {
        let v3 = v0();
      } catch(v28) {
        eval(\`
            v13(v11,RegExp);
        \`);
      }
    }
    new Promise(v0);
  }
  main();
  gczeal(7,1);
`);
