// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

const {GlobalOverrider, FakePrefs} = require("test/unit/utils");
const {TelemetrySender, TelemetrySenderConstants} = require("lib/TelemetrySender.jsm");
const {ENDPOINT_PREF, FHR_UPLOAD_ENABLED_PREF, TELEMETRY_PREF, LOGGING_PREF} =
  TelemetrySenderConstants;

/**
 * A reference to the fake preferences object created by the TelemetrySender
 * constructor so that we can use the API.
 */
let fakePrefs;
const prefInitHook = function() {
  fakePrefs = this; // eslint-disable-line consistent-this
};

const tsArgs = {prefInitHook};

describe("TelemetrySender", () => {
  let globals;
  let tSender;
  let sandbox;
  let fetchStub;
  const fakeEndpointUrl = "http://127.0.0.1/stuff";
  const fakePingJSON = JSON.stringify({action: "fake_action", monkey: 1});
  const fakeFetchHttpErrorResponse = {ok: false, status: 400};
  const fakeFetchSuccessResponse = {ok: true, status: 200};

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    fetchStub = sandbox.stub();

    globals.set("Preferences", FakePrefs);
    globals.set("fetch", fetchStub);
    sandbox.spy(global.Components.utils, "reportError");
  });

  afterEach(() => {
    globals.restore();
    FakePrefs.prototype.prefs = {};
  });

  it("should construct the Prefs object", () => {
    globals.sandbox.spy(global, "Preferences");

    tSender = new TelemetrySender(tsArgs);

    assert.calledOnce(global.Preferences);
  });

  describe("#enabled", () => {
    let testParams = [
      {enabledPref: true, fhrPref: true, result: true},
      {enabledPref: false, fhrPref: true, result: false},
      {enabledPref: true, fhrPref: false, result: false},
      {enabledPref: false, fhrPref: false, result: false}
    ];

    function testEnabled(p) {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = p.enabledPref;
      FakePrefs.prototype.prefs[FHR_UPLOAD_ENABLED_PREF] = p.fhrPref;

      tSender = new TelemetrySender(tsArgs);

      assert.equal(tSender.enabled, p.result);
    }

    for (let p of testParams) {
      it(`should return ${p.result} if the fhrPref is ${p.fhrPref} and telemetry.enabled is ${p.enabledPref}`, () => {
        testEnabled(p);
      });
    }

    describe("telemetry.enabled pref changes from true to false", () => {
      beforeEach(() => {
        FakePrefs.prototype.prefs = {};
        FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
        FakePrefs.prototype.prefs[FHR_UPLOAD_ENABLED_PREF] = true;
        tSender = new TelemetrySender(tsArgs);
        assert.propertyVal(tSender, "enabled", true);
      });

      it("should set the enabled property to false", () => {
        fakePrefs.set(TELEMETRY_PREF, false);

        assert.propertyVal(tSender, "enabled", false);
      });
    });

    describe("telemetry.enabled pref changes from false to true", () => {
      beforeEach(() => {
        FakePrefs.prototype.prefs = {};
        FakePrefs.prototype.prefs[FHR_UPLOAD_ENABLED_PREF] = true;
        FakePrefs.prototype.prefs[TELEMETRY_PREF] = false;
        tSender = new TelemetrySender(tsArgs);

        assert.propertyVal(tSender, "enabled", false);
      });

      it("should set the enabled property to true", () => {
        fakePrefs.set(TELEMETRY_PREF, true);

        assert.propertyVal(tSender, "enabled", true);
      });
    });

    describe("FHR enabled pref changes from true to false", () => {
      beforeEach(() => {
        FakePrefs.prototype.prefs = {};
        FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
        FakePrefs.prototype.prefs[FHR_UPLOAD_ENABLED_PREF] = true;
        tSender = new TelemetrySender(tsArgs);
        assert.propertyVal(tSender, "enabled", true);
      });

      it("should set the enabled property to false", () => {
        fakePrefs.set(FHR_UPLOAD_ENABLED_PREF, false);

        assert.propertyVal(tSender, "enabled", false);
      });
    });

    describe("FHR enabled pref changes from false to true", () => {
      beforeEach(() => {
        FakePrefs.prototype.prefs = {};
        FakePrefs.prototype.prefs[FHR_UPLOAD_ENABLED_PREF] = false;
        FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
        tSender = new TelemetrySender(tsArgs);

        assert.propertyVal(tSender, "enabled", false);
      });

      it("should set the enabled property to true", () => {
        fakePrefs.set(FHR_UPLOAD_ENABLED_PREF, true);

        assert.propertyVal(tSender, "enabled", true);
      });
    });
  });

  describe("#sendPing()", () => {
    beforeEach(() => {
      FakePrefs.prototype.prefs = {};
      FakePrefs.prototype.prefs[FHR_UPLOAD_ENABLED_PREF] = true;
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      FakePrefs.prototype.prefs[ENDPOINT_PREF] = fakeEndpointUrl;
      tSender = new TelemetrySender(tsArgs);
    });

    it("should not send if the TelemetrySender is disabled", async () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = false;
      tSender = new TelemetrySender(tsArgs);

      await tSender.sendPing(fakePingJSON);

      assert.notCalled(fetchStub);
    });

    it("should POST given ping data to telemetry.ping.endpoint pref w/fetch",
    async () => {
      fetchStub.resolves(fakeFetchSuccessResponse);
      await tSender.sendPing(fakePingJSON);

      assert.calledOnce(fetchStub);
      assert.calledWithExactly(fetchStub, fakeEndpointUrl,
        {method: "POST", body: JSON.stringify(fakePingJSON)});
    });

    it("should log HTTP failures using Cu.reportError", async () => {
      fetchStub.resolves(fakeFetchHttpErrorResponse);

      await tSender.sendPing(fakePingJSON);

      assert.called(Components.utils.reportError);
    });

    it("should log an error using Cu.reportError if fetch rejects", async () => {
      fetchStub.rejects("Oh noes!");

      await tSender.sendPing(fakePingJSON);

      assert.called(Components.utils.reportError);
    });

    it("should log if logging is on && if action is not activity_stream_performance", async () => {
      globals.sandbox.stub(console, "log");
      FakePrefs.prototype.prefs = {};
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      FakePrefs.prototype.prefs[LOGGING_PREF] =  true;
      fetchStub.resolves(fakeFetchSuccessResponse);
      tSender = new TelemetrySender(tsArgs);

      await tSender.sendPing(fakePingJSON);

      assert.called(console.log); // eslint-disable-line no-console
    });
  });

  describe("#uninit()", () => {
    it("should remove the telemetry pref listener", () => {
      tSender = new TelemetrySender(tsArgs);
      assert.property(fakePrefs.observers, TELEMETRY_PREF);

      tSender.uninit();

      assert.notProperty(fakePrefs.observers, TELEMETRY_PREF);
    });

    it("should remove the fhrpref listener", () => {
      tSender = new TelemetrySender(tsArgs);
      assert.property(fakePrefs.observers, FHR_UPLOAD_ENABLED_PREF);

      tSender.uninit();

      assert.notProperty(fakePrefs.observers, FHR_UPLOAD_ENABLED_PREF);
    });

    it("should remove the telemetry log listener", () => {
      tSender = new TelemetrySender(tsArgs);
      assert.property(fakePrefs.observers, LOGGING_PREF);

      tSender.uninit();

      assert.notProperty(fakePrefs.observers, TELEMETRY_PREF);
    });

    it("should call Cu.reportError if this._prefs.ignore throws", () => {
      globals.sandbox.stub(FakePrefs.prototype, "ignore").throws("Some Error");
      tSender = new TelemetrySender(tsArgs);

      tSender.uninit();

      assert.called(global.Components.utils.reportError);
    });
  });

  describe("Misc pref changes", () => {
    describe("performance.log changes from false to true", () => {
      it("should change this.logging from false to true", () => {
        FakePrefs.prototype.prefs = {};
        FakePrefs.prototype.prefs[LOGGING_PREF] = false;
        tSender = new TelemetrySender(tsArgs);
        assert.propertyVal(tSender, "logging", false);

        fakePrefs.set(LOGGING_PREF, true);

        assert.propertyVal(tSender, "logging", true);
      });
    });
  });
});
