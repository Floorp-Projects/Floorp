// |jit-test| allow-oom; skip-if: !('oomTest' in this)

loadFile(`
  switch (0) {
    case (-1):
  }
`);
function loadFile(lfVarx) {
  oomTest(function() {
      let m = parseModule(lfVarx);
  });
}
