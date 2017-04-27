const {GlobalOverrider} = require("test/unit/utils");

const req = require.context(".", true, /\.test\.js$/);
const files = req.keys();

// This exposes sinon assertions to chai.assert
sinon.assert.expose(assert, {prefix: ""});

let overrider = new GlobalOverrider();
overrider.set({
  Components: {
    interfaces: {},
    utils: {
      import: overrider.sandbox.spy(),
      importGlobalProperties: overrider.sandbox.spy(),
      reportError: overrider.sandbox.spy()
    }
  },
  XPCOMUtils: {
    defineLazyModuleGetter: overrider.sandbox.spy(),
    defineLazyServiceGetter: overrider.sandbox.spy(),
    generateQI: overrider.sandbox.stub().returns(() => {})
  },
  console: {log: overrider.sandbox.spy()},
  dump: overrider.sandbox.spy(),
  Services: {
    obs: {
      addObserver: overrider.sandbox.spy(),
      removeObserver: overrider.sandbox.spy()
    }
  }
});

describe("activity-stream", () => {
  afterEach(() => overrider.reset());
  after(() => overrider.restore());
  files.forEach(file => req(file));
});
