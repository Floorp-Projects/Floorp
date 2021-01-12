// |jit-test| --ion-offthread-compile=off; --enable-private-methods; skip-if: !('oomTest' in this)

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