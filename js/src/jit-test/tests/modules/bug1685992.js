// |jit-test| --ion-offthread-compile=off; skip-if: !('oomTest' in this)

function oomModule(lfMod) {
  oomTest(function () {
    parseModule(lfMod);
  });
}
oomModule(`
  class B50 {
    #priv() {}
  }
`)