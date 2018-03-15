import {EventEmitter, FakePerformance, FakePrefs, GlobalOverrider} from "test/unit/utils";
import Adapter from "enzyme-adapter-react-16";
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
  AddonManager: {
    getActiveAddons() {
      return Promise.resolve({addons: [], fullData: false});
    }
  },
  AppConstants: {MOZILLA_OFFICIAL: true},
  ChromeUtils: {
    defineModuleGetter() {},
    import() {}
  },
  Components: {isSuccessCode: () => true},
  // eslint-disable-next-line object-shorthand
  ContentSearchUIController: function() {}, // NB: This is a function/constructor
  Cc: {},
  Ci: {nsIHttpChannel: {REFERRER_POLICY_UNSAFE_URL: 5}},
  Cu: {
    importGlobalProperties() {},
    now: () => window.performance.now(),
    reportError() {}
  },
  dump() {},
  fetch() {},
  // eslint-disable-next-line object-shorthand
  Image: function() {}, // NB: This is a function/constructor
  PluralForm: {get() {}},
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
    telemetry: {
      setEventRecordingEnabled: () => {},
      recordEvent: eventDetails => {}
    },
    console: {logStringMessage: () => {}},
    prefs: {
      addObserver() {},
      prefHasUserValue() {},
      removeObserver() {},
      getStringPref() {},
      getIntPref() {},
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
    io: {
      newURI: spec => ({
        mutate: () => ({
          setRef: ref => ({
            finalize: () => ({
              ref,
              spec
            })
          })
        }),
        spec
      })
    },
    search: {
      init(cb) { cb(); },
      getVisibleEngines: () => [{identifier: "google"}, {identifier: "bing"}],
      defaultEngine: {identifier: "google"}
    },
    scriptSecurityManager: {
      createNullPrincipal() {},
      getSystemPrincipal() {}
    }
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
