function loadX(lfVarx) {
    oomTest(function() {
        let m55 = parseModule(lfVarx);
    });
}
loadX(`
  class B50 {
    #priv() {}
  }
`)
