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
    classes: {},
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
    telemetry: {
      canRecordBase: true,
      canRecordExtended: true
    },
    locale: {
      getAppLocalesAsLangTags() {},
      getRequestedLocale() {},
      negotiateLanguages() {}
    },
    urlFormatter: {formatURL: str => str},
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
      addObserver() {},
      prefHasUserValue() {},
      removeObserver() {},
      getStringPref() {},
      getBoolPref() {},
      getDefaultBranch() {
        return {
          setBoolPref() {},
          setIntPref() {},
          setStringPref() {},
          clearUserPref() {}
        };
      }
    },
    tm: {dispatchToMainThread: cb => cb()},
    eTLD: {getPublicSuffix() {}},
    io: {NewURI() {}}
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
