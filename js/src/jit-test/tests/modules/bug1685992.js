// |jit-test| --ion-offthread-compile=off

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
