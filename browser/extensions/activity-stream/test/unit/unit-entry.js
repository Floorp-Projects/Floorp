const {GlobalOverrider, FakePrefs, FakePerformance} = require("test/unit/utils");
const {chaiAssertions} = require("test/schemas/pings");

const req = require.context(".", true, /\.test\.jsx?$/);
const files = req.keys();

// This exposes sinon assertions to chai.assert
sinon.assert.expose(assert, {prefix: ""});

chai.use(chaiAssertions);

let overrider = new GlobalOverrider();

overrider.set({
  Components: {
    interfaces: {},
    utils: {
      import() {},
      importGlobalProperties() {},
      reportError() {},
      now: () => window.performance.now()
    }
  },
  // eslint-disable-next-line object-shorthand
  ContentSearchUIController: function() {}, // NB: This is a function/constructor
  dump() {},
  fetch() {},
  Preferences: FakePrefs,
  Services: {
    locale: {getRequestedLocale() {}},
    mm: {
      addMessageListener: (msg, cb) => cb(),
      removeMessageListener() {}
    },
    appShell: {hiddenDOMWindow: {performance: new FakePerformance()}},
    obs: {
      addObserver() {},
      removeObserver() {}
    },
    prefs: {
      getDefaultBranch() {
        return {
          setBoolPref() {},
          setIntPref() {},
          setStringPref() {},
          clearUserPref() {}
        };
      }
    }
  },
  XPCOMUtils: {
    defineLazyModuleGetter() {},
    defineLazyServiceGetter() {},
    generateQI() { return {}; }
  }
});

describe("activity-stream", () => {
  after(() => overrider.restore());
  files.forEach(file => req(file));
});
