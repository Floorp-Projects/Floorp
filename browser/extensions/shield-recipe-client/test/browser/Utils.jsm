const {utils: Cu} = Components;
Cu.import("resource://shield-recipe-client/lib/SandboxManager.jsm", this);
Cu.import("resource://shield-recipe-client/lib/NormandyDriver.jsm", this);

this.EXPORTED_SYMBOLS = ["Utils"];

this.Utils = {
  UUID_REGEX: /[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}/,

  withSandboxManager(Assert, testGenerator) {
    return function* inner() {
      const sandboxManager = new SandboxManager();
      sandboxManager.addHold("test running");

      yield testGenerator(sandboxManager);

      sandboxManager.removeHold("test running");
      yield sandboxManager.isNuked()
        .then(() => Assert.ok(true, "sandbox is nuked"))
        .catch(e => Assert.ok(false, "sandbox is nuked", e));
    };
  },

  withDriver(Assert, testGenerator) {
    return Utils.withSandboxManager(Assert, function* inner(sandboxManager) {
      const driver = new NormandyDriver(sandboxManager);
      yield testGenerator(driver);
    });
  },
};
