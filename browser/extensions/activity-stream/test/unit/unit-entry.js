const {GlobalOverrider, FakePrefs, FakePerformance, EventEmitter} = require("test/unit/utils");
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
    },
    isSuccessCode: () => true
  },
  // eslint-disable-next-line object-shorthand
  ContentSearchUIController: function() {}, // NB: This is a function/constructor
  dump() {},
  fetch() {},
  // eslint-disable-next-line object-shorthand
  Image: function() {}, // NB: This is a function/constructor
  Preferences: FakePrefs,
  Services: {
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
    console: {logStringMessage: () => {}},
    prefs: {
      addObserver() {},
      prefHasUserValue() {},
      removeObserver() {},
      getStringPref() {},
      getBoolPref() {},
      getBranch() {},
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
    eTLD: {
      getBaseDomain({spec}) { return spec.match(/\/([^/]+)/)[1]; },
      getPublicSuffix() {}
    },
    io: {newURI(url) { return {spec: url}; }},
    search: {
      init(cb) { cb(); },
      getVisibleEngines: () => [{identifier: "google"}, {identifier: "bing"}],
      defaultEngine: {identifier: "google"}
    }
  },
  XPCOMUtils: {
    defineLazyModuleGetter() {},
    defineLazyServiceGetter() {},
    generateQI() { return {}; }
  },
  EventEmitter,
  ShellService: {isDefaultBrowser: () => true}
});

describe("activity-stream", () => {
  after(() => overrider.restore());
  files.forEach(file => req(file));
});
