// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

const {GlobalOverrider, FakePrefs} = require("test/unit/utils");
const {TelemetrySender} = require("lib/TelemetrySender.jsm");

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
  let fetchStub;
  const observerTopics = ["user-action-event", "performance-event",
    "tab-session-complete", "undesired-event"];
  const fakeEndpointUrl = "http://127.0.0.1/stuff";
  const fakePingJSON = JSON.stringify({action: "fake_action", monkey: 1});
  const fakeFetchHttpErrorResponse = {ok: false, status: 400};
  const fakeFetchSuccessResponse = {ok: true, status: 200};

  function assertNotificationObserversAdded() {
    observerTopics.forEach(topic => {
      assert.calledWithExactly(
        global.Services.obs.addObserver, tSender, topic, true);
    });
  }

  function assertNotificationObserversRemoved() {
    observerTopics.forEach(topic => {
      assert.calledWithExactly(
        global.Services.obs.removeObserver, tSender, topic);
    });
  }

  before(() => {
    globals = new GlobalOverrider();

    fetchStub = globals.sandbox.stub();

    globals.set("Preferences", FakePrefs);
    globals.set("fetch", fetchStub);
  });

  beforeEach(() => {
  });

  afterEach(() => {
    globals.reset();
    FakePrefs.prototype.prefs = {};
  });

  after(() => globals.restore());

  it("should construct the Prefs object with the right branch", () => {
    globals.sandbox.spy(global, "Preferences");

    tSender = new TelemetrySender(tsArgs);

    assert.calledOnce(global.Preferences);
    assert.calledWith(global.Preferences,
      sinon.match.has("branch", "browser.newtabpage.activity-stream"));
  });

  it("should set the enabled prop to false if the pref is false", () => {
    FakePrefs.prototype.prefs = {telemetry: false};

    tSender = new TelemetrySender(tsArgs);

    assert.isFalse(tSender.enabled);
  });

  it("should not add notification observers if the enabled pref is false", () => {
    FakePrefs.prototype.prefs = {telemetry: false};

    tSender = new TelemetrySender(tsArgs);

    assert.notCalled(global.Services.obs.addObserver);
  });

  it("should set the enabled prop to true if the pref is true", () => {
    FakePrefs.prototype.prefs = {telemetry: true};

    tSender = new TelemetrySender(tsArgs);

    assert.isTrue(tSender.enabled);
  });

  it("should add all notification observers if the enabled pref is true", () => {
    FakePrefs.prototype.prefs = {telemetry: true};

    tSender = new TelemetrySender(tsArgs);

    assertNotificationObserversAdded();
  });

  describe("#_sendPing()", () => {
    beforeEach(() => {
      FakePrefs.prototype.prefs = {
        "telemetry": true,
        "telemetry.ping.endpoint": fakeEndpointUrl
      };
      tSender = new TelemetrySender(tsArgs);
    });

    it("should POST given ping data to telemetry.ping.endpoint pref w/fetch",
    async () => {
      fetchStub.resolves(fakeFetchSuccessResponse);
      await tSender._sendPing(fakePingJSON);

      assert.calledOnce(fetchStub);
      assert.calledWithExactly(fetchStub, fakeEndpointUrl,
        {method: "POST", body: fakePingJSON});
    });

    it("should log HTTP failures using Cu.reportError", async () => {
      fetchStub.resolves(fakeFetchHttpErrorResponse);

      await tSender._sendPing(fakePingJSON);

      assert.called(Components.utils.reportError);
    });

    it("should log an error using Cu.reportError if fetch rejects", async () => {
      fetchStub.rejects("Oh noes!");

      await tSender._sendPing(fakePingJSON);

      assert.called(Components.utils.reportError);
    });

    it("should log if logging is on && if action is not activity_stream_performance", async () => {
      FakePrefs.prototype.prefs = {
        "telemetry": true,
        "performance.log": true
      };
      fetchStub.resolves(fakeFetchSuccessResponse);
      tSender = new TelemetrySender(tsArgs);

      await tSender._sendPing(fakePingJSON);

      assert.called(console.log); // eslint-disable-line no-console
    });
  });

  describe("#observe()", () => {
    before(() => {
      globals.sandbox.stub(TelemetrySender.prototype, "_sendPing");
    });

    observerTopics.forEach(topic => {
      it(`should call this._sendPing with data for ${topic}`, () => {
        const fakeSubject = "fakeSubject";
        tSender = new TelemetrySender(tsArgs);

        tSender.observe(fakeSubject, topic, fakePingJSON);

        assert.calledOnce(TelemetrySender.prototype._sendPing);
        assert.calledWithExactly(TelemetrySender.prototype._sendPing,
          fakePingJSON);
      });
    });

    it("should not call this._sendPing for 'nonexistent-topic'", () => {
      const fakeSubject = "fakeSubject";
      tSender = new TelemetrySender(tsArgs);

      tSender.observe(fakeSubject, "nonexistent-topic", fakePingJSON);

      assert.notCalled(TelemetrySender.prototype._sendPing);
    });
  });

  describe("#uninit()", () => {
    it("should remove the telemetry pref listener", () => {
      tSender = new TelemetrySender(tsArgs);
      assert.property(fakePrefs.observers, "telemetry");

      tSender.uninit();

      assert.notProperty(fakePrefs.observers, "telemetry");
    });

    it("should remove all notification observers if telemetry pref is true", () => {
      FakePrefs.prototype.prefs = {telemetry: true};
      tSender = new TelemetrySender(tsArgs);

      tSender.uninit();

      assertNotificationObserversRemoved();
    });

    it("should not remove notification observers if telemetry pref is false", () => {
      FakePrefs.prototype.prefs = {telemetry: false};
      tSender = new TelemetrySender(tsArgs);

      tSender.uninit();

      assert.notCalled(global.Services.obs.removeObserver);
    });

    it("should call Cu.reportError if this._prefs.ignore throws", () => {
      globals.sandbox.stub(FakePrefs.prototype, "ignore").throws("Some Error");
      tSender = new TelemetrySender(tsArgs);

      tSender.uninit();

      assert.called(global.Components.utils.reportError);
    });
  });

  describe("Misc pref changes", () => {
    describe("telemetry changes from true to false", () => {
      beforeEach(() => {
        FakePrefs.prototype.prefs = {"telemetry": true};
        tSender = new TelemetrySender(tsArgs);
        assert.propertyVal(tSender, "enabled", true);
      });

      it("should set the enabled property to false", () => {
        fakePrefs.set("telemetry", false);

        assert.propertyVal(tSender, "enabled", false);
      });

      it("should remove all notification observers", () => {
        fakePrefs.set("telemetry", false);

        assertNotificationObserversRemoved();
      });
    });

    describe("telemetry changes from false to true", () => {
      beforeEach(() => {
        FakePrefs.prototype.prefs = {"telemetry": false};
        tSender = new TelemetrySender(tsArgs);
        assert.propertyVal(tSender, "enabled", false);
      });

      it("should set the enabled property to true", () => {
        fakePrefs.set("telemetry", true);

        assert.propertyVal(tSender, "enabled", true);
      });

      it("should add all topic observers", () => {
        fakePrefs.set("telemetry", true);

        assertNotificationObserversAdded();
      });
    });

    describe("performance.log changes from false to true", () => {
      it("should change this.logging from false to true", () => {
        FakePrefs.prototype.prefs = {"performance.log": false};
        tSender = new TelemetrySender(tsArgs);
        assert.propertyVal(tSender, "logging", false);

        fakePrefs.set("performance.log", true);

        assert.propertyVal(tSender, "logging", true);
      });
    });
  });
});
