// |jit-test| skip-if: !('oomTest' in this)
oomTest(function() {
  let m = parseModule(`x = a?.b; x = a?.b; x = a?.b;`);
});
