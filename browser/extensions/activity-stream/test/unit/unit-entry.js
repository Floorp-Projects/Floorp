import {EventEmitter, FakePerformance, FakePrefs, GlobalOverrider} from "test/unit/utils";
import Adapter from "enzyme-adapter-react-15";
import {chaiAssertions} from "test/schemas/pings";
import enzyme from "enzyme";

enzyme.configure({adapter: new Adapter()});

// Cause React warnings to make tests that trigger them fail
const origConsoleError = console.error; // eslint-disable-line no-console
console.error = function(msg, ...args) { // eslint-disable-line no-console
  // eslint-disable-next-line no-console
  origConsoleError.apply(console, [msg, ...args]);

  if (/(Invalid prop|Failed prop type|Check the render method|React Intl)/.test(msg)) {
    throw new Error(msg);
  }
};

const req = require.context(".", true, /\.test\.jsx?$/);
const files = req.keys();

// This exposes sinon assertions to chai.assert
sinon.assert.expose(assert, {prefix: ""});

chai.use(chaiAssertions);

let overrider = new GlobalOverrider();

overrider.set({
  AppConstants: {MOZILLA_OFFICIAL: true},
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
      getAppLocaleAsLangTag() { return "en-US"; },
      getAppLocalesAsLangTags() {},
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
    },
    scriptSecurityManager: {getSystemPrincipal() {}}
  },
  XPCOMUtils: {
    defineLazyGetter(_1, _2, f) { f(); },
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
