/* global Services */
import {
  actionCreators as ac,
  actionTypes as at,
  actionUtils as au,
} from "common/Actions.jsm";
import {
  ASRouterEventPing,
  BasePing,
  ImpressionStatsPing,
  SessionPing,
  UserEventPing,
} from "test/schemas/pings";
import { FakePrefs, GlobalOverrider } from "test/unit/utils";
import { ASRouterPreferences } from "lib/ASRouterPreferences.jsm";
import injector from "inject!lib/TelemetryFeed.jsm";
import { MESSAGE_TYPE_HASH as msg } from "common/ActorConstants.jsm";

const FAKE_UUID = "{foo-123-foo}";
const FAKE_ROUTER_MESSAGE_PROVIDER = [{ id: "cfr", enabled: true }];
const FAKE_TELEMETRY_ID = "foo123";

// eslint-disable-next-line max-statements
describe("TelemetryFeed", () => {
  let globals;
  let sandbox;
  let expectedUserPrefs;
  let browser = {
    getAttribute() {
      return "true";
    },
  };
  let instance;
  let clock;
  let fakeHomePageUrl;
  let fakeHomePage;
  let fakeExtensionSettingsStore;
  let ExperimentAPI = { getExperimentMetaData: () => {} };
  class PingCentre {
    sendPing() {}
    uninit() {}
    sendStructuredIngestionPing() {}
  }
  class UTEventReporting {
    sendUserEvent() {}
    sendSessionEndEvent() {}
    uninit() {}
  }

  const {
    TelemetryFeed,
    USER_PREFS_ENCODING,
    PREF_IMPRESSION_ID,
    TELEMETRY_PREF,
    EVENTS_TELEMETRY_PREF,
    STRUCTURED_INGESTION_ENDPOINT_PREF,
  } = injector({
    "lib/UTEventReporting.jsm": { UTEventReporting },
  });

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    clock = sinon.useFakeTimers();
    fakeHomePageUrl = "about:home";
    fakeHomePage = {
      get() {
        return fakeHomePageUrl;
      },
    };
    fakeExtensionSettingsStore = {
      initialize() {
        return Promise.resolve();
      },
      getSetting() {},
    };
    sandbox.spy(global.Cu, "reportError");
    globals.set("AboutNewTab", {
      newTabURLOverridden: false,
      newTabURL: "",
    });
    globals.set("HomePage", fakeHomePage);
    globals.set("ExtensionSettingsStore", fakeExtensionSettingsStore);
    globals.set("PingCentre", PingCentre);
    globals.set("UTEventReporting", UTEventReporting);
    globals.set("ClientID", {
      getClientID: sandbox.spy(async () => FAKE_TELEMETRY_ID),
    });
    globals.set("ExperimentAPI", ExperimentAPI);

    sandbox
      .stub(ASRouterPreferences, "providers")
      .get(() => FAKE_ROUTER_MESSAGE_PROVIDER);
    instance = new TelemetryFeed();
  });
  afterEach(() => {
    clock.restore();
    globals.restore();
    FakePrefs.prototype.prefs = {};
    ASRouterPreferences.uninit();
  });
  describe("#init", () => {
    it("should create an instance", () => {
      const testInstance = new TelemetryFeed();
      assert.isDefined(testInstance);
    });
    it("should add .pingCentre, a PingCentre instance", () => {
      assert.instanceOf(instance.pingCentre, PingCentre);
    });
    it("should add .utEvents, a UTEventReporting instance", () => {
      assert.instanceOf(instance.utEvents, UTEventReporting);
    });
    it("should make this.browserOpenNewtabStart() observe browser-open-newtab-start", () => {
      sandbox.spy(Services.obs, "addObserver");

      instance.init();

      assert.calledTwice(Services.obs.addObserver);
      assert.calledWithExactly(
        Services.obs.addObserver,
        instance.browserOpenNewtabStart,
        "browser-open-newtab-start"
      );
    });
    it("should add window open listener", () => {
      sandbox.spy(Services.obs, "addObserver");

      instance.init();

      assert.calledTwice(Services.obs.addObserver);
      assert.calledWithExactly(
        Services.obs.addObserver,
        instance._addWindowListeners,
        "domwindowopened"
      );
    });
    it("should add TabPinned event listener on new windows", () => {
      const stub = { addEventListener: sandbox.stub() };
      sandbox.spy(Services.obs, "addObserver");

      instance.init();

      assert.calledTwice(Services.obs.addObserver);
      const [cb] = Services.obs.addObserver.secondCall.args;
      cb(stub);
      assert.calledTwice(stub.addEventListener);
      assert.calledWithExactly(
        stub.addEventListener,
        "unload",
        instance.handleEvent
      );
      assert.calledWithExactly(
        stub.addEventListener,
        "TabPinned",
        instance.handleEvent
      );
    });
    it("should create impression id if none exists", () => {
      assert.equal(instance._impressionId, FAKE_UUID);
    });
    it("should set impression id if it exists", () => {
      FakePrefs.prototype.prefs = {};
      FakePrefs.prototype.prefs[PREF_IMPRESSION_ID] = "fakeImpressionId";
      assert.equal(new TelemetryFeed()._impressionId, "fakeImpressionId");
    });
    it("should register listeners on existing windows", () => {
      const stub = sandbox.stub();
      globals.set({
        Services: {
          ...Services,
          wm: { getEnumerator: () => [{ addEventListener: stub }] },
        },
      });

      instance.init();

      assert.calledTwice(stub);
      assert.calledWithExactly(stub, "unload", instance.handleEvent);
      assert.calledWithExactly(stub, "TabPinned", instance.handleEvent);
    });
    describe("telemetry pref changes from false to true", () => {
      beforeEach(() => {
        FakePrefs.prototype.prefs = {};
        FakePrefs.prototype.prefs[TELEMETRY_PREF] = false;
        instance = new TelemetryFeed();

        assert.propertyVal(instance, "telemetryEnabled", false);
      });

      it("should set the enabled property to true", () => {
        instance._prefs.set(TELEMETRY_PREF, true);

        assert.propertyVal(instance, "telemetryEnabled", true);
      });
    });
    describe("events telemetry pref changes from false to true", () => {
      beforeEach(() => {
        FakePrefs.prototype.prefs = {};
        FakePrefs.prototype.prefs[EVENTS_TELEMETRY_PREF] = false;
        instance = new TelemetryFeed();

        assert.propertyVal(instance, "eventTelemetryEnabled", false);
      });

      it("should set the enabled property to true", () => {
        instance._prefs.set(EVENTS_TELEMETRY_PREF, true);

        assert.propertyVal(instance, "eventTelemetryEnabled", true);
      });
    });
    it("should set two scalars for deletion-request", () => {
      sandbox.spy(Services.telemetry, "scalarSet");

      instance.init();

      assert.calledTwice(Services.telemetry.scalarSet);

      // impression_id
      let [type, value] = Services.telemetry.scalarSet.firstCall.args;
      assert.equal(type, "deletion.request.impression_id");
      assert.equal(value, instance._impressionId);

      // context_id
      [type, value] = Services.telemetry.scalarSet.secondCall.args;
      assert.equal(type, "deletion.request.context_id");
      assert.equal(value, FAKE_UUID);
    });
  });
  describe("#handleEvent", () => {
    it("should dispatch a TAB_PINNED_EVENT", () => {
      sandbox.stub(instance, "sendEvent");
      globals.set({
        Services: {
          ...Services,
          wm: {
            getEnumerator: () => [{ gBrowser: { tabs: [{ pinned: true }] } }],
          },
        },
      });

      instance.handleEvent({ type: "TabPinned", target: {} });

      assert.calledOnce(instance.sendEvent);
      const [ping] = instance.sendEvent.firstCall.args;
      assert.propertyVal(ping, "event", "TABPINNED");
      assert.propertyVal(ping, "source", "TAB_CONTEXT_MENU");
      assert.propertyVal(ping, "session_id", "n/a");
      assert.propertyVal(ping.value, "total_pinned_tabs", 1);
    });
    it("should skip private windows", () => {
      sandbox.stub(instance, "sendEvent");
      globals.set({ PrivateBrowsingUtils: { isWindowPrivate: () => true } });

      instance.handleEvent({ type: "TabPinned", target: {} });

      assert.notCalled(instance.sendEvent);
    });
    it("should return the correct value for total_pinned_tabs", () => {
      sandbox.stub(instance, "sendEvent");
      globals.set({
        Services: {
          ...Services,
          wm: {
            getEnumerator: () => [
              {
                gBrowser: { tabs: [{ pinned: true }, { pinned: false }] },
              },
            ],
          },
        },
      });

      instance.handleEvent({ type: "TabPinned", target: {} });

      assert.calledOnce(instance.sendEvent);
      const [ping] = instance.sendEvent.firstCall.args;
      assert.propertyVal(ping, "event", "TABPINNED");
      assert.propertyVal(ping, "source", "TAB_CONTEXT_MENU");
      assert.propertyVal(ping, "session_id", "n/a");
      assert.propertyVal(ping.value, "total_pinned_tabs", 1);
    });
    it("should return the correct value for total_pinned_tabs (when private windows are open)", () => {
      sandbox.stub(instance, "sendEvent");
      const privateWinStub = sandbox
        .stub()
        .onCall(0)
        .returns(false)
        .onCall(1)
        .returns(true);
      globals.set({
        PrivateBrowsingUtils: { isWindowPrivate: privateWinStub },
      });
      globals.set({
        Services: {
          ...Services,
          wm: {
            getEnumerator: () => [
              {
                gBrowser: { tabs: [{ pinned: true }, { pinned: true }] },
              },
            ],
          },
        },
      });

      instance.handleEvent({ type: "TabPinned", target: {} });

      assert.calledOnce(instance.sendEvent);
      const [ping] = instance.sendEvent.firstCall.args;
      assert.propertyVal(ping.value, "total_pinned_tabs", 0);
    });
    it("should unregister the event listeners", () => {
      const stub = { removeEventListener: sandbox.stub() };

      instance.handleEvent({ type: "unload", target: stub });

      assert.calledTwice(stub.removeEventListener);
      assert.calledWithExactly(
        stub.removeEventListener,
        "unload",
        instance.handleEvent
      );
      assert.calledWithExactly(
        stub.removeEventListener,
        "TabPinned",
        instance.handleEvent
      );
    });
  });
  describe("#addSession", () => {
    it("should add a session and return it", () => {
      const session = instance.addSession("foo");

      assert.equal(instance.sessions.get("foo"), session);
    });
    it("should set the session_id", () => {
      sandbox.spy(Services.uuid, "generateUUID");

      const session = instance.addSession("foo");

      assert.calledOnce(Services.uuid.generateUUID);
      assert.equal(
        session.session_id,
        Services.uuid.generateUUID.firstCall.returnValue
      );
    });
    it("should set the page if a url parameter is given", () => {
      const session = instance.addSession("foo", "about:monkeys");

      assert.propertyVal(session, "page", "about:monkeys");
    });
    it("should set the page prop to 'unknown' if no URL parameter given", () => {
      const session = instance.addSession("foo");

      assert.propertyVal(session, "page", "unknown");
    });
    it("should set the perf type when lacking timestamp", () => {
      const session = instance.addSession("foo");

      assert.propertyVal(session.perf, "load_trigger_type", "unexpected");
    });
    it("should set load_trigger_type to first_window_opened on the first about:home seen", () => {
      const session = instance.addSession("foo", "about:home");

      assert.propertyVal(
        session.perf,
        "load_trigger_type",
        "first_window_opened"
      );
    });
    it("should not set load_trigger_type to first_window_opened on the second about:home seen", () => {
      instance.addSession("foo", "about:home");

      const session2 = instance.addSession("foo", "about:home");

      assert.notPropertyVal(
        session2.perf,
        "load_trigger_type",
        "first_window_opened"
      );
    });
    it("should set load_trigger_ts to the value of the process start timestamp", () => {
      const session = instance.addSession("foo", "about:home");

      assert.propertyVal(session.perf, "load_trigger_ts", 1588010448000);
    });
    it("should create a valid session ping on the first about:home seen", () => {
      // Add a session
      const portID = "foo";
      const session = instance.addSession(portID, "about:home");

      // Create a ping referencing the session
      const ping = instance.createSessionEndEvent(session);
      assert.validate(ping, SessionPing);
    });
    it("should be a valid ping with the data_late_by_ms perf", () => {
      // Add a session
      const portID = "foo";
      const session = instance.addSession(portID, "about:home");
      instance.saveSessionPerfData("foo", { topsites_data_late_by_ms: 10 });
      instance.saveSessionPerfData("foo", { highlights_data_late_by_ms: 20 });

      // Create a ping referencing the session
      const ping = instance.createSessionEndEvent(session);
      assert.validate(ping, SessionPing);
      assert.propertyVal(
        instance.sessions.get("foo").perf,
        "highlights_data_late_by_ms",
        20
      );
      assert.propertyVal(
        instance.sessions.get("foo").perf,
        "topsites_data_late_by_ms",
        10
      );
    });
    it("should be a valid ping with the topsites stats perf", () => {
      // Add a session
      const portID = "foo";
      const session = instance.addSession(portID, "about:home");
      instance.saveSessionPerfData("foo", {
        topsites_icon_stats: {
          custom_screenshot: 0,
          screenshot_with_icon: 2,
          screenshot: 1,
          tippytop: 2,
          rich_icon: 1,
          no_image: 0,
        },
        topsites_pinned: 3,
        topsites_search_shortcuts: 2,
      });

      // Create a ping referencing the session
      const ping = instance.createSessionEndEvent(session);
      assert.validate(ping, SessionPing);
      assert.propertyVal(
        instance.sessions.get("foo").perf.topsites_icon_stats,
        "screenshot_with_icon",
        2
      );
      assert.equal(instance.sessions.get("foo").perf.topsites_pinned, 3);
      assert.equal(
        instance.sessions.get("foo").perf.topsites_search_shortcuts,
        2
      );
    });
  });

  describe("#browserOpenNewtabStart", () => {
    it("should call ChromeUtils.addProfilerMarker with browser-open-newtab-start", () => {
      globals.set("ChromeUtils", {
        addProfilerMarker: sandbox.stub(),
      });

      sandbox.stub(global.Cu, "now").returns(12345);

      instance.browserOpenNewtabStart();

      assert.calledOnce(ChromeUtils.addProfilerMarker);
      assert.calledWithExactly(
        ChromeUtils.addProfilerMarker,
        "UserTiming",
        12345,
        "browser-open-newtab-start"
      );
    });
  });

  describe("#endSession", () => {
    it("should not throw if there is no session for the given port ID", () => {
      assert.doesNotThrow(() => instance.endSession("doesn't exist"));
    });
    it("should add a session_duration integer if there is a visibility_event_rcvd_ts", () => {
      sandbox.stub(instance, "sendEvent");
      const session = instance.addSession("foo");
      session.perf.visibility_event_rcvd_ts = 444.4732;

      instance.endSession("foo");

      assert.isNumber(session.session_duration);
      assert.ok(
        Number.isInteger(session.session_duration),
        "session_duration should be an integer"
      );
    });
    it("shouldn't send session ping if there's no visibility_event_rcvd_ts", () => {
      sandbox.stub(instance, "sendEvent");
      instance.addSession("foo");

      instance.endSession("foo");

      assert.notCalled(instance.sendEvent);
      assert.isFalse(instance.sessions.has("foo"));
    });
    it("should remove the session from .sessions", () => {
      sandbox.stub(instance, "sendEvent");
      instance.addSession("foo");

      instance.endSession("foo");

      assert.isFalse(instance.sessions.has("foo"));
    });
    it("should call createSessionSendEvent and sendEvent with the sesssion", () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      FakePrefs.prototype.prefs[EVENTS_TELEMETRY_PREF] = true;
      instance = new TelemetryFeed();

      sandbox.stub(instance, "sendEvent");
      sandbox.stub(instance, "createSessionEndEvent");
      sandbox.stub(instance.utEvents, "sendSessionEndEvent");
      const session = instance.addSession("foo");
      session.perf.visibility_event_rcvd_ts = 444.4732;

      instance.endSession("foo");

      // Did we call sendEvent with the result of createSessionEndEvent?
      assert.calledWith(instance.createSessionEndEvent, session);

      let sessionEndEvent =
        instance.createSessionEndEvent.firstCall.returnValue;
      assert.calledWith(instance.sendEvent, sessionEndEvent);
      assert.calledWith(instance.utEvents.sendSessionEndEvent, sessionEndEvent);
    });
  });
  describe("ping creators", () => {
    beforeEach(() => {
      FakePrefs.prototype.prefs = {};
      for (const pref of Object.keys(USER_PREFS_ENCODING)) {
        FakePrefs.prototype.prefs[pref] = true;
        expectedUserPrefs |= USER_PREFS_ENCODING[pref];
      }
      instance.init();
    });
    describe("#createPing", () => {
      it("should create a valid base ping without a session if no portID is supplied", async () => {
        const ping = await instance.createPing();
        assert.validate(ping, BasePing);
        assert.notProperty(ping, "session_id");
        assert.notProperty(ping, "page");
      });
      it("should create a valid base ping with session info if a portID is supplied", async () => {
        // Add a session
        const portID = "foo";
        instance.addSession(portID, "about:home");
        const sessionID = instance.sessions.get(portID).session_id;

        // Create a ping referencing the session
        const ping = await instance.createPing(portID);
        assert.validate(ping, BasePing);

        // Make sure we added the right session-related stuff to the ping
        assert.propertyVal(ping, "session_id", sessionID);
        assert.propertyVal(ping, "page", "about:home");
      });
      it("should create an unexpected base ping if no session yet portID is supplied", async () => {
        const ping = await instance.createPing("foo");

        assert.validate(ping, BasePing);
        assert.propertyVal(ping, "page", "unknown");
        assert.propertyVal(
          instance.sessions.get("foo").perf,
          "load_trigger_type",
          "unexpected"
        );
      });
      it("should create a base ping with user_prefs", async () => {
        const ping = await instance.createPing("foo");

        assert.validate(ping, BasePing);
        assert.propertyVal(ping, "user_prefs", expectedUserPrefs);
      });
    });
    describe("#createUserEvent", () => {
      it("should create a valid event", async () => {
        const portID = "foo";
        const data = { source: "TOP_SITES", event: "CLICK" };
        const action = ac.AlsoToMain(ac.UserEvent(data), portID);
        const session = instance.addSession(portID);

        const ping = await instance.createUserEvent(action);

        // Is it valid?
        assert.validate(ping, UserEventPing);
        // Does it have the right session_id?
        assert.propertyVal(ping, "session_id", session.session_id);
      });
    });
    describe("#createSessionEndEvent", () => {
      it("should create a valid event", async () => {
        const ping = await instance.createSessionEndEvent({
          session_id: FAKE_UUID,
          page: "about:newtab",
          session_duration: 12345,
          perf: {
            load_trigger_ts: 10,
            load_trigger_type: "menu_plus_or_keyboard",
            visibility_event_rcvd_ts: 20,
            is_preloaded: true,
          },
        });

        // Is it valid?
        assert.validate(ping, SessionPing);
        assert.propertyVal(ping, "session_id", FAKE_UUID);
        assert.propertyVal(ping, "page", "about:newtab");
        assert.propertyVal(ping, "session_duration", 12345);
      });
      it("should create a valid unexpected session event", async () => {
        const ping = await instance.createSessionEndEvent({
          session_id: FAKE_UUID,
          page: "about:newtab",
          session_duration: 12345,
          perf: {
            load_trigger_type: "unexpected",
            is_preloaded: true,
          },
        });

        // Is it valid?
        assert.validate(ping, SessionPing);
        assert.propertyVal(ping, "session_id", FAKE_UUID);
        assert.propertyVal(ping, "page", "about:newtab");
        assert.propertyVal(ping, "session_duration", 12345);
        assert.propertyVal(ping.perf, "load_trigger_type", "unexpected");
      });
    });
  });
  describe("#createImpressionStats", () => {
    it("should create a valid impression stats ping", async () => {
      const tiles = [{ id: 10001 }, { id: 10002 }, { id: 10003 }];
      const action = ac.ImpressionStats({ source: "POCKET", tiles });
      const ping = await instance.createImpressionStats(
        au.getPortIdOfSender(action),
        action.data
      );

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "source", "POCKET");
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should create a valid click ping", async () => {
      const tiles = [{ id: 10001, pos: 2 }];
      const action = ac.ImpressionStats({ source: "POCKET", tiles, click: 0 });
      const ping = await instance.createImpressionStats(
        au.getPortIdOfSender(action),
        action.data
      );

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "click", 0);
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should create a valid block ping", async () => {
      const tiles = [{ id: 10001, pos: 2 }];
      const action = ac.ImpressionStats({ source: "POCKET", tiles, block: 0 });
      const ping = await instance.createImpressionStats(
        au.getPortIdOfSender(action),
        action.data
      );

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "block", 0);
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should create a valid pocket ping", async () => {
      const tiles = [{ id: 10001, pos: 2 }];
      const action = ac.ImpressionStats({ source: "POCKET", tiles, pocket: 0 });
      const ping = await instance.createImpressionStats(
        au.getPortIdOfSender(action),
        action.data
      );

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "pocket", 0);
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should pass shim if it is available to impression ping", async () => {
      const tiles = [{ id: 10001, pos: 2, shim: 1234 }];
      const action = ac.ImpressionStats({ source: "POCKET", tiles });
      const ping = await instance.createImpressionStats(
        au.getPortIdOfSender(action),
        action.data
      );

      assert.propertyVal(ping, "tiles", tiles);
      assert.propertyVal(ping.tiles[0], "shim", tiles[0].shim);
    });
    it("should not include client_id and session_id", async () => {
      const tiles = [{ id: 10001 }, { id: 10002 }, { id: 10003 }];
      const action = ac.ImpressionStats({ source: "POCKET", tiles });
      const ping = await instance.createImpressionStats(
        au.getPortIdOfSender(action),
        action.data
      );

      assert.validate(ping, ImpressionStatsPing);
      assert.notProperty(ping, "client_id");
      assert.notProperty(ping, "session_id");
    });
  });
  describe("#applyCFRPolicy", () => {
    it("should use client_id and message_id in prerelease", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "nightly";
        },
      });
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const { ping, pingType } = await instance.applyCFRPolicy(data);

      assert.equal(pingType, "cfr");
      assert.isUndefined(ping.impression_id);
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "bucket_id", "cfr_bucket_01");
      assert.propertyVal(ping, "message_id", "cfr_message_01");
    });
    it("should use impression_id and bucket_id in release", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const { ping, pingType } = await instance.applyCFRPolicy(data);

      assert.equal(pingType, "cfr");
      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", FAKE_UUID);
      assert.propertyVal(ping, "message_id", "n/a");
      assert.propertyVal(ping, "bucket_id", "cfr_bucket_01");
    });
    it("should use client_id and message_id in the experiment cohort in release", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
        slug: "SOME-CFR-EXP",
      });
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const { ping, pingType } = await instance.applyCFRPolicy(data);

      assert.equal(pingType, "cfr");
      assert.isUndefined(ping.impression_id);
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "bucket_id", "cfr_bucket_01");
      assert.propertyVal(ping, "message_id", "cfr_message_01");
    });
  });
  describe("#applyWhatsNewPolicy", () => {
    it("should set client_id and set pingType", async () => {
      const { ping, pingType } = await instance.applyWhatsNewPolicy({});

      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.equal(pingType, "whats-new-panel");
    });
  });
  describe("#applyInfoBarPolicy", () => {
    it("should set client_id and set pingType", async () => {
      const { ping, pingType } = await instance.applyInfoBarPolicy({});

      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.equal(pingType, "infobar");
    });
  });
  describe("#applySpotlightPolicy", () => {
    it("should set client_id and set pingType", async () => {
      let pingData = { action: "foo" };
      const { ping, pingType } = await instance.applySpotlightPolicy(pingData);

      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.equal(pingType, "spotlight");
      assert.notProperty(ping, "action");
    });
  });
  describe("#applyMomentsPolicy", () => {
    it("should use client_id and message_id in prerelease", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "nightly";
        },
      });
      const data = {
        action: "moments_user_event",
        event: "IMPRESSION",
        message_id: "moments_message_01",
        bucket_id: "moments_bucket_01",
      };
      const { ping, pingType } = await instance.applyMomentsPolicy(data);

      assert.equal(pingType, "moments");
      assert.isUndefined(ping.impression_id);
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "bucket_id", "moments_bucket_01");
      assert.propertyVal(ping, "message_id", "moments_message_01");
    });
    it("should use impression_id and bucket_id in release", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      const data = {
        action: "moments_user_event",
        event: "IMPRESSION",
        message_id: "moments_message_01",
        bucket_id: "moments_bucket_01",
      };
      const { ping, pingType } = await instance.applyMomentsPolicy(data);

      assert.equal(pingType, "moments");
      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", FAKE_UUID);
      assert.propertyVal(ping, "message_id", "n/a");
      assert.propertyVal(ping, "bucket_id", "moments_bucket_01");
    });
    it("should use client_id and message_id in the experiment cohort in release", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
        slug: "SOME-CFR-EXP",
      });
      const data = {
        action: "moments_user_event",
        event: "IMPRESSION",
        message_id: "moments_message_01",
        bucket_id: "moments_bucket_01",
      };
      const { ping, pingType } = await instance.applyMomentsPolicy(data);

      assert.equal(pingType, "moments");
      assert.isUndefined(ping.impression_id);
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "bucket_id", "moments_bucket_01");
      assert.propertyVal(ping, "message_id", "moments_message_01");
    });
  });
  describe("#applySnippetsPolicy", () => {
    it("should include client_id", async () => {
      const data = {
        action: "snippets_user_event",
        event: "IMPRESSION",
        message_id: "snippets_message_01",
      };
      const { ping, pingType } = await instance.applySnippetsPolicy(data);

      assert.equal(pingType, "snippets");
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "message_id", "snippets_message_01");
    });
  });
  describe("#applyOnboardingPolicy", () => {
    it("should include client_id", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
      };
      const { ping, pingType } = await instance.applyOnboardingPolicy(data);

      assert.equal(pingType, "onboarding");
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
      assert.propertyVal(ping, "browser_session_id", "fake_session_id");
    });
    it("should include page to event_context if there is a session", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
      };
      const session = { page: "about:welcome" };
      const { ping, pingType } = await instance.applyOnboardingPolicy(
        data,
        session
      );

      assert.equal(pingType, "onboarding");
      assert.propertyVal(
        ping,
        "event_context",
        JSON.stringify({ page: "about:welcome" })
      );
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
    });
    it("should not set page if it is not in ONBOARDING_ALLOWED_PAGE_VALUES", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
      };
      const session = { page: "foo" };
      const { ping, pingType } = await instance.applyOnboardingPolicy(
        data,
        session
      );

      assert.calledOnce(global.Cu.reportError);
      assert.equal(pingType, "onboarding");
      assert.propertyVal(ping, "event_context", JSON.stringify({}));
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
    });
    it("should append page to event_context if it is not empty", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
        event_context: JSON.stringify({ foo: "bar" }),
      };
      const session = { page: "about:welcome" };
      const { ping, pingType } = await instance.applyOnboardingPolicy(
        data,
        session
      );

      assert.equal(pingType, "onboarding");
      assert.propertyVal(
        ping,
        "event_context",
        JSON.stringify({ foo: "bar", page: "about:welcome" })
      );
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
    });
    it("should append page to event_context if it is not a JSON serialized string", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
        event_context: "foo",
      };
      const session = { page: "about:welcome" };
      const { ping, pingType } = await instance.applyOnboardingPolicy(
        data,
        session
      );

      assert.equal(pingType, "onboarding");
      assert.propertyVal(
        ping,
        "event_context",
        JSON.stringify({ value: "foo", page: "about:welcome" })
      );
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
    });
  });
  describe("#applyUndesiredEventPolicy", () => {
    it("should exclude client_id and use impression_id", () => {
      const data = {
        action: "asrouter_undesired_event",
        event: "RS_MISSING_DATA",
      };
      const { ping, pingType } = instance.applyUndesiredEventPolicy(data);

      assert.equal(pingType, "undesired-events");
      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", FAKE_UUID);
    });
  });
  describe("#createASRouterEvent", () => {
    it("should create a valid AS Router event", async () => {
      const data = {
        action: "snippets_user_event",
        event: "CLICK",
        message_id: "snippets_message_01",
      };
      const action = ac.ASRouterUserEvent(data);
      const { ping } = await instance.createASRouterEvent(action);

      assert.validate(ping, ASRouterEventPing);
      assert.propertyVal(ping, "event", "CLICK");
    });
    it("should call applyCFRPolicy if action equals to cfr_user_event", async () => {
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        message_id: "cfr_message_01",
      };
      sandbox.stub(instance, "applyCFRPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyCFRPolicy);
    });
    it("should call applySnippetsPolicy if action equals to snippets_user_event", async () => {
      const data = {
        action: "snippets_user_event",
        event: "IMPRESSION",
        message_id: "snippets_message_01",
      };
      sandbox.stub(instance, "applySnippetsPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applySnippetsPolicy);
    });
    it("should call applySnippetsPolicy if action equals to snippets_local_testing_user_event", async () => {
      const data = {
        action: "snippets_local_testing_user_event",
        event: "IMPRESSION",
        message_id: "snippets_message_01",
      };
      sandbox.stub(instance, "applySnippetsPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applySnippetsPolicy);
    });
    it("should call applyOnboardingPolicy if action equals to onboarding_user_event", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTON",
        message_id: "onboarding_message_01",
      };
      sandbox.stub(instance, "applyOnboardingPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyOnboardingPolicy);
    });
    it("should call applyWhatsNewPolicy if action equals to whats-new-panel_user_event", async () => {
      const data = {
        action: "whats-new-panel_user_event",
        event: "CLICK_BUTTON",
        message_id: "whats-new-panel_message_01",
      };
      sandbox.stub(instance, "applyWhatsNewPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyWhatsNewPolicy);
    });
    it("should call applyMomentsPolicy if action equals to moments_user_event", async () => {
      const data = {
        action: "moments_user_event",
        event: "CLICK_BUTTON",
        message_id: "moments_message_01",
      };
      sandbox.stub(instance, "applyMomentsPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyMomentsPolicy);
    });
    it("should call applySpotlightPolicy if action equals to spotlight_user_event", async () => {
      const data = {
        action: "spotlight_user_event",
        event: "CLICK",
        message_id: "SPOTLIGHT_MESSAGE_93",
      };
      sandbox.stub(instance, "applySpotlightPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applySpotlightPolicy);
    });
    it("should call applyUndesiredEventPolicy if action equals to asrouter_undesired_event", async () => {
      const data = {
        action: "asrouter_undesired_event",
        event: "UNDESIRED_EVENT",
      };
      sandbox.stub(instance, "applyUndesiredEventPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyUndesiredEventPolicy);
    });
    it("should stringify event_context if it is an Object", async () => {
      const data = {
        action: "asrouter_undesired_event",
        event: "UNDESIRED_EVENT",
        event_context: { foo: "bar" },
      };
      const action = ac.ASRouterUserEvent(data);
      const { ping } = await instance.createASRouterEvent(action);

      assert.propertyVal(ping, "event_context", JSON.stringify({ foo: "bar" }));
    });
    it("should not stringify event_context if it is a String", async () => {
      const data = {
        action: "asrouter_undesired_event",
        event: "UNDESIRED_EVENT",
        event_context: "foo",
      };
      const action = ac.ASRouterUserEvent(data);
      const { ping } = await instance.createASRouterEvent(action);

      assert.propertyVal(ping, "event_context", "foo");
    });
  });
  describe("#sendEventPing", () => {
    it("should call sendStructuredIngestionEvent", async () => {
      const data = {
        action: "activity_stream_user_event",
        event: "CLICK",
      };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendStructuredIngestionEvent");

      await instance.sendEventPing(data);

      const expectedPayload = {
        client_id: FAKE_TELEMETRY_ID,
        event: "CLICK",
        browser_session_id: "fake_session_id",
      };
      assert.calledWith(instance.sendStructuredIngestionEvent, expectedPayload);
    });
    it("should stringify value if it is an Object", async () => {
      const data = {
        action: "activity_stream_user_event",
        event: "CLICK",
        value: { foo: "bar" },
      };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendStructuredIngestionEvent");

      await instance.sendEventPing(data);

      const expectedPayload = {
        client_id: FAKE_TELEMETRY_ID,
        event: "CLICK",
        browser_session_id: "fake_session_id",
        value: JSON.stringify({ foo: "bar" }),
      };
      assert.calledWith(instance.sendStructuredIngestionEvent, expectedPayload);
    });
  });
  describe("#sendSessionPing", () => {
    it("should call sendStructuredIngestionEvent", async () => {
      const data = {
        action: "activity_stream_session",
        page: "about:home",
        session_duration: 10000,
      };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendStructuredIngestionEvent");

      await instance.sendSessionPing(data);

      const expectedPayload = {
        client_id: FAKE_TELEMETRY_ID,
        page: "about:home",
        session_duration: 10000,
      };
      assert.calledWith(instance.sendStructuredIngestionEvent, expectedPayload);
    });
  });
  describe("#sendEvent", () => {
    it("should call sendEventPing on activity_stream_user_event", () => {
      FakePrefs.prototype.prefs.telemetry = true;
      const event = { action: "activity_stream_user_event" };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendEventPing");

      instance.sendEvent(event);

      assert.calledOnce(instance.sendEventPing);
    });
    it("should call sendSessionPing on activity_stream_session", () => {
      FakePrefs.prototype.prefs.telemetry = true;
      const event = { action: "activity_stream_session" };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendSessionPing");

      instance.sendEvent(event);

      assert.calledOnce(instance.sendSessionPing);
    });
  });
  describe("#sendUTEvent", () => {
    it("should call the UT event function passed in", async () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      FakePrefs.prototype.prefs[EVENTS_TELEMETRY_PREF] = true;
      const event = {};
      instance = new TelemetryFeed();
      sandbox.stub(instance.utEvents, "sendUserEvent");

      await instance.sendUTEvent(event, instance.utEvents.sendUserEvent);

      assert.calledWith(instance.utEvents.sendUserEvent, event);
    });
  });
  describe("#sendStructuredIngestionEvent", () => {
    it("should call PingCentre sendStructuredIngestionPing", async () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      const event = {};
      instance = new TelemetryFeed();
      sandbox.stub(instance.pingCentre, "sendStructuredIngestionPing");

      await instance.sendStructuredIngestionEvent(
        event,
        "http://foo.com/base/"
      );

      assert.calledWith(instance.pingCentre.sendStructuredIngestionPing, event);
    });
  });
  describe("#setLoadTriggerInfo", () => {
    it("should call saveSessionPerfData w/load_trigger_{ts,type} data", () => {
      sandbox.stub(global.Cu, "now").returns(12345);

      globals.set("ChromeUtils", {
        addProfilerMarker: sandbox.stub(),
      });

      instance.browserOpenNewtabStart();

      const stub = sandbox.stub(instance, "saveSessionPerfData");
      instance.addSession("port123");

      instance.setLoadTriggerInfo("port123");

      assert.calledWith(stub, "port123", {
        load_trigger_ts: 1588010448000 + 12345,
        load_trigger_type: "menu_plus_or_keyboard",
      });
    });

    it("should not call saveSessionPerfData when getting mark throws", () => {
      const stub = sandbox.stub(instance, "saveSessionPerfData");
      instance.addSession("port123");

      instance.setLoadTriggerInfo("port123");

      assert.notCalled(stub);
    });
  });

  describe("#saveSessionPerfData", () => {
    it("should update the given session with the given data", () => {
      instance.addSession("port123");
      assert.notProperty(instance.sessions.get("port123"), "fake_ts");
      const data = { fake_ts: 456, other_fake_ts: 789 };

      instance.saveSessionPerfData("port123", data);

      assert.include(instance.sessions.get("port123").perf, data);
    });

    it("should call setLoadTriggerInfo if data has visibility_event_rcvd_ts", () => {
      sandbox.stub(instance, "setLoadTriggerInfo");
      instance.addSession("port123");
      const data = { visibility_event_rcvd_ts: 444455 };

      instance.saveSessionPerfData("port123", data);

      assert.calledOnce(instance.setLoadTriggerInfo);
      assert.calledWithExactly(instance.setLoadTriggerInfo, "port123");
      assert.include(instance.sessions.get("port123").perf, data);
    });

    it("shouldn't call setLoadTriggerInfo if data has no visibility_event_rcvd_ts", () => {
      sandbox.stub(instance, "setLoadTriggerInfo");
      instance.addSession("port123");

      instance.saveSessionPerfData("port123", { monkeys_ts: 444455 });

      assert.notCalled(instance.setLoadTriggerInfo);
    });

    it("should not call setLoadTriggerInfo when url is about:home", () => {
      sandbox.stub(instance, "setLoadTriggerInfo");
      instance.addSession("port123", "about:home");
      const data = { visibility_event_rcvd_ts: 444455 };

      instance.saveSessionPerfData("port123", data);

      assert.notCalled(instance.setLoadTriggerInfo);
    });

    it("should call maybeRecordTopsitesPainted when url is about:home and topsites_first_painted_ts is given", () => {
      const topsites_first_painted_ts = 44455;
      const data = { topsites_first_painted_ts };
      const spy = sandbox.spy();

      sandbox.stub(Services.prefs, "getIntPref").returns(1);
      globals.set("AboutNewTab", {
        maybeRecordTopsitesPainted: spy,
      });
      instance.addSession("port123", "about:home");
      instance.saveSessionPerfData("port123", data);

      assert.calledOnce(spy);
      assert.calledWith(spy, topsites_first_painted_ts);
    });
  });
  describe("#uninit", () => {
    it("should call .pingCentre.uninit", () => {
      const stub = sandbox.stub(instance.pingCentre, "uninit");

      instance.uninit();

      assert.calledOnce(stub);
    });
    it("should call .utEvents.uninit", () => {
      const stub = sandbox.stub(instance.utEvents, "uninit");

      instance.uninit();

      assert.calledOnce(stub);
    });
    it("should make this.browserOpenNewtabStart() stop observing browser-open-newtab-start and domwindowopened", async () => {
      await instance.init();
      sandbox.spy(Services.obs, "removeObserver");
      sandbox.stub(instance.pingCentre, "uninit");

      await instance.uninit();

      assert.calledTwice(Services.obs.removeObserver);
      assert.calledWithExactly(
        Services.obs.removeObserver,
        instance.browserOpenNewtabStart,
        "browser-open-newtab-start"
      );
      assert.calledWithExactly(
        Services.obs.removeObserver,
        instance._addWindowListeners,
        "domwindowopened"
      );
    });
  });
  describe("#onAction", () => {
    beforeEach(() => {
      FakePrefs.prototype.prefs = {};
    });
    it("should call .init() on an INIT action", () => {
      const init = sandbox.stub(instance, "init");
      const sendPageTakeoverData = sandbox.stub(
        instance,
        "sendPageTakeoverData"
      );

      instance.onAction({ type: at.INIT });

      assert.calledOnce(init);
      assert.calledOnce(sendPageTakeoverData);
    });
    it("should call .uninit() on an UNINIT action", () => {
      const stub = sandbox.stub(instance, "uninit");

      instance.onAction({ type: at.UNINIT });

      assert.calledOnce(stub);
    });
    it("should call .handleNewTabInit on a NEW_TAB_INIT action", () => {
      sandbox.spy(instance, "handleNewTabInit");

      instance.onAction(
        ac.AlsoToMain({
          type: at.NEW_TAB_INIT,
          data: { url: "about:newtab", browser },
        })
      );

      assert.calledOnce(instance.handleNewTabInit);
    });
    it("should call .addSession() on a NEW_TAB_INIT action", () => {
      const stub = sandbox.stub(instance, "addSession").returns({ perf: {} });
      sandbox.stub(instance, "setLoadTriggerInfo");

      instance.onAction(
        ac.AlsoToMain(
          {
            type: at.NEW_TAB_INIT,
            data: { url: "about:monkeys", browser },
          },
          "port123"
        )
      );

      assert.calledOnce(stub);
      assert.calledWith(stub, "port123", "about:monkeys");
    });
    it("should call .endSession() on a NEW_TAB_UNLOAD action", () => {
      const stub = sandbox.stub(instance, "endSession");

      instance.onAction(ac.AlsoToMain({ type: at.NEW_TAB_UNLOAD }, "port123"));

      assert.calledWith(stub, "port123");
    });
    it("should call .saveSessionPerfData on SAVE_SESSION_PERF_DATA", () => {
      const stub = sandbox.stub(instance, "saveSessionPerfData");
      const data = { some_ts: 10 };
      const action = { type: at.SAVE_SESSION_PERF_DATA, data };

      instance.onAction(ac.AlsoToMain(action, "port123"));

      assert.calledWith(stub, "port123", data);
    });
    it("should send an event on a TELEMETRY_USER_EVENT action", () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      FakePrefs.prototype.prefs[EVENTS_TELEMETRY_PREF] = true;
      instance = new TelemetryFeed();

      const sendEvent = sandbox.stub(instance, "sendEvent");
      const utSendUserEvent = sandbox.stub(instance.utEvents, "sendUserEvent");
      const eventCreator = sandbox.stub(instance, "createUserEvent");
      const action = { type: at.TELEMETRY_USER_EVENT };

      instance.onAction(action);

      assert.calledWith(eventCreator, action);
      assert.calledWith(sendEvent, eventCreator.returnValue);
      assert.calledWith(utSendUserEvent, eventCreator.returnValue);
    });
    describe("should call handleASRouterUserEvent on x action", () => {
      const actions = [
        at.AS_ROUTER_TELEMETRY_USER_EVENT,
        msg.TOOLBAR_BADGE_TELEMETRY,
        msg.TOOLBAR_PANEL_TELEMETRY,
        msg.MOMENTS_PAGE_TELEMETRY,
        msg.DOORHANGER_TELEMETRY,
      ];
      actions.forEach(type => {
        it(`${type} action`, () => {
          FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
          FakePrefs.prototype.prefs[EVENTS_TELEMETRY_PREF] = true;
          instance = new TelemetryFeed();

          const eventHandler = sandbox.spy(instance, "handleASRouterUserEvent");
          const action = {
            type,
            data: { event: "CLICK" },
          };

          instance.onAction(action);

          assert.calledWith(eventHandler, action);
        });
      });
    });
    it("should send an event on a TELEMETRY_IMPRESSION_STATS action", () => {
      const sendEvent = sandbox.stub(instance, "sendStructuredIngestionEvent");
      const eventCreator = sandbox.stub(instance, "createImpressionStats");
      const tiles = [{ id: 10001 }, { id: 10002 }, { id: 10003 }];
      const action = ac.ImpressionStats({ source: "POCKET", tiles });

      instance.onAction(action);

      assert.calledWith(
        eventCreator,
        au.getPortIdOfSender(action),
        action.data
      );
      assert.calledWith(sendEvent, eventCreator.returnValue);
    });
    it("should call .handleDiscoveryStreamImpressionStats on a DISCOVERY_STREAM_IMPRESSION_STATS action", () => {
      const session = {};
      sandbox.stub(instance.sessions, "get").returns(session);
      const data = { source: "foo", tiles: [{ id: 1 }] };
      const action = { type: at.DISCOVERY_STREAM_IMPRESSION_STATS, data };
      sandbox.spy(instance, "handleDiscoveryStreamImpressionStats");

      instance.onAction(ac.AlsoToMain(action, "port123"));

      assert.calledWith(
        instance.handleDiscoveryStreamImpressionStats,
        "port123",
        data
      );
    });
    it("should call .handleDiscoveryStreamLoadedContent on a DISCOVERY_STREAM_LOADED_CONTENT action", () => {
      const session = {};
      sandbox.stub(instance.sessions, "get").returns(session);
      const data = { source: "foo", tiles: [{ id: 1 }] };
      const action = { type: at.DISCOVERY_STREAM_LOADED_CONTENT, data };
      sandbox.spy(instance, "handleDiscoveryStreamLoadedContent");

      instance.onAction(ac.AlsoToMain(action, "port123"));

      assert.calledWith(
        instance.handleDiscoveryStreamLoadedContent,
        "port123",
        data
      );
    });
    it("should call .handleTopSitesImpressionStats on a TOP_SITES_IMPRESSION_STATS action", () => {
      const session = {};
      sandbox.stub(instance.sessions, "get").returns(session);
      const data = { type: "impression", tile_id: 42, position: 1 };
      const action = { type: at.TOP_SITES_IMPRESSION_STATS, data };
      sandbox.spy(instance, "handleTopSitesImpressionStats");

      instance.onAction(ac.AlsoToMain(action));

      assert.calledOnce(instance.handleTopSitesImpressionStats);
      assert.deepEqual(
        instance.handleTopSitesImpressionStats.firstCall.args[0].data,
        data
      );
    });
  });
  describe("#handleNewTabInit", () => {
    it("should set the session as preloaded if the browser is preloaded", () => {
      const session = { perf: {} };
      let preloadedBrowser = {
        getAttribute() {
          return "preloaded";
        },
      };
      sandbox.stub(instance, "addSession").returns(session);

      instance.onAction(
        ac.AlsoToMain({
          type: at.NEW_TAB_INIT,
          data: { url: "about:newtab", browser: preloadedBrowser },
        })
      );

      assert.ok(session.perf.is_preloaded);
    });
    it("should set the session as non-preloaded if the browser is non-preloaded", () => {
      const session = { perf: {} };
      let nonPreloadedBrowser = {
        getAttribute() {
          return "";
        },
      };
      sandbox.stub(instance, "addSession").returns(session);

      instance.onAction(
        ac.AlsoToMain({
          type: at.NEW_TAB_INIT,
          data: { url: "about:newtab", browser: nonPreloadedBrowser },
        })
      );

      assert.ok(!session.perf.is_preloaded);
    });
  });
  describe("#SendASRouterUndesiredEvent", () => {
    it("should call handleASRouterUserEvent", () => {
      let stub = sandbox.stub(instance, "handleASRouterUserEvent");

      instance.SendASRouterUndesiredEvent({ foo: "bar" });

      assert.calledOnce(stub);
      let [payload] = stub.firstCall.args;
      assert.propertyVal(payload.data, "action", "asrouter_undesired_event");
      assert.propertyVal(payload.data, "foo", "bar");
    });
  });
  describe("#sendPageTakeoverData", () => {
    let fakePrefs = { "browser.newtabpage.enabled": true };

    beforeEach(() => {
      globals.set(
        "Services",
        Object.assign({}, Services, {
          prefs: { getBoolPref: key => fakePrefs[key] },
        })
      );
      // Services.prefs = {getBoolPref: key => fakePrefs[key]};
    });
    it("should send correct event data for about:home set to custom URL", async () => {
      fakeHomePageUrl = "https://searchprovider.com";
      instance._prefs.set(TELEMETRY_PREF, true);
      instance._classifySite = () => Promise.resolve("other");
      const sendEvent = sandbox.stub(instance, "sendEvent");

      await instance.sendPageTakeoverData();
      assert.calledOnce(sendEvent);
      assert.equal(sendEvent.firstCall.args[0].event, "PAGE_TAKEOVER_DATA");
      assert.deepEqual(sendEvent.firstCall.args[0].value, {
        home_url_category: "other",
      });
      assert.validate(sendEvent.firstCall.args[0], UserEventPing);
    });
    it("should send correct event data for about:newtab set to custom URL", async () => {
      globals.set("AboutNewTab", {
        newTabURLOverridden: true,
        newTabURL: "https://searchprovider.com",
      });
      instance._prefs.set(TELEMETRY_PREF, true);
      instance._classifySite = () => Promise.resolve("other");
      const sendEvent = sandbox.stub(instance, "sendEvent");

      await instance.sendPageTakeoverData();
      assert.calledOnce(sendEvent);
      assert.equal(sendEvent.firstCall.args[0].event, "PAGE_TAKEOVER_DATA");
      assert.deepEqual(sendEvent.firstCall.args[0].value, {
        newtab_url_category: "other",
      });
      assert.validate(sendEvent.firstCall.args[0], UserEventPing);
    });
    it("should not send an event if neither about:{home,newtab} are set to custom URL", async () => {
      instance._prefs.set(TELEMETRY_PREF, true);
      const sendEvent = sandbox.stub(instance, "sendEvent");

      await instance.sendPageTakeoverData();
      assert.notCalled(sendEvent);
    });
    it("should send home_extension_id and newtab_extension_id when appropriate", async () => {
      const ID = "{abc-foo-bar}";
      fakeExtensionSettingsStore.getSetting = () => ({ id: ID });
      instance._prefs.set(TELEMETRY_PREF, true);
      instance._classifySite = () => Promise.resolve("other");
      const sendEvent = sandbox.stub(instance, "sendEvent");

      await instance.sendPageTakeoverData();
      assert.calledOnce(sendEvent);
      assert.equal(sendEvent.firstCall.args[0].event, "PAGE_TAKEOVER_DATA");
      assert.deepEqual(sendEvent.firstCall.args[0].value, {
        home_extension_id: ID,
        newtab_extension_id: ID,
      });
      assert.validate(sendEvent.firstCall.args[0], UserEventPing);
    });
  });
  describe("#sendDiscoveryStreamImpressions", () => {
    it("should not send impression pings if there is no impression data", () => {
      const spy = sandbox.spy(instance, "sendEvent");
      const session = {};
      instance.sendDiscoveryStreamImpressions("foo", session);

      assert.notCalled(spy);
    });
    it("should send impression pings if there is impression data", () => {
      const spy = sandbox.spy(instance, "sendStructuredIngestionEvent");
      const session = {
        impressionSets: {
          source_foo: [
            { id: 1, pos: 0 },
            { id: 2, pos: 1 },
          ],
          source_bar: [
            { id: 3, pos: 0 },
            { id: 4, pos: 1 },
          ],
        },
      };
      instance.sendDiscoveryStreamImpressions("foo", session);

      assert.calledTwice(spy);
    });
  });
  describe("#sendDiscoveryStreamLoadedContent", () => {
    it("should not send loaded content pings if there is no loaded content data", () => {
      const spy = sandbox.spy(instance, "sendEvent");
      const session = {};
      instance.sendDiscoveryStreamLoadedContent("foo", session);

      assert.notCalled(spy);
    });
    it("should send loaded content pings if there is loaded content data", () => {
      const spy = sandbox.spy(instance, "sendStructuredIngestionEvent");
      const session = {
        loadedContentSets: {
          source_foo: [
            { id: 1, pos: 0 },
            { id: 2, pos: 1 },
          ],
          source_bar: [
            { id: 3, pos: 0 },
            { id: 4, pos: 1 },
          ],
        },
      };
      instance.sendDiscoveryStreamLoadedContent("foo", session);

      assert.calledTwice(spy);

      let [payload] = spy.firstCall.args;
      let sources = new Set([]);
      sources.add(payload.source);
      assert.equal(payload.loaded, 2);
      assert.deepEqual(
        payload.tiles,
        session.loadedContentSets[payload.source]
      );

      [payload] = spy.secondCall.args;
      sources.add(payload.source);
      assert.equal(payload.loaded, 2);
      assert.deepEqual(
        payload.tiles,
        session.loadedContentSets[payload.source]
      );

      assert.deepEqual(sources, new Set(["source_foo", "source_bar"]));
    });
  });
  describe("#handleDiscoveryStreamImpressionStats", () => {
    it("should throw for a missing session", () => {
      assert.throws(() => {
        instance.handleDiscoveryStreamImpressionStats("a_missing_port", {});
      }, "Session does not exist.");
    });
    it("should store impression to impressionSets", () => {
      const session = instance.addSession("new_session", "about:newtab");
      instance.handleDiscoveryStreamImpressionStats("new_session", {
        source: "foo",
        tiles: [{ id: 1, pos: 0 }],
        window_inner_width: 1000,
        window_inner_height: 900,
      });

      assert.equal(Object.keys(session.impressionSets).length, 1);
      assert.deepEqual(session.impressionSets.foo, {
        tiles: [{ id: 1, pos: 0 }],
        window_inner_width: 1000,
        window_inner_height: 900,
      });

      // Add another ping with the same source
      instance.handleDiscoveryStreamImpressionStats("new_session", {
        source: "foo",
        tiles: [{ id: 2, pos: 1 }],
        window_inner_width: 1000,
        window_inner_height: 900,
      });

      assert.deepEqual(session.impressionSets.foo, {
        tiles: [
          { id: 1, pos: 0 },
          { id: 2, pos: 1 },
        ],
        window_inner_width: 1000,
        window_inner_height: 900,
      });

      // Add another ping with a different source
      instance.handleDiscoveryStreamImpressionStats("new_session", {
        source: "bar",
        tiles: [{ id: 3, pos: 2 }],
        window_inner_width: 1000,
        window_inner_height: 900,
      });

      assert.equal(Object.keys(session.impressionSets).length, 2);
      assert.deepEqual(session.impressionSets.foo, {
        tiles: [
          { id: 1, pos: 0 },
          { id: 2, pos: 1 },
        ],
        window_inner_width: 1000,
        window_inner_height: 900,
      });
      assert.deepEqual(session.impressionSets.bar, {
        tiles: [{ id: 3, pos: 2 }],
        window_inner_width: 1000,
        window_inner_height: 900,
      });
    });
  });
  describe("#handleDiscoveryStreamLoadedContent", () => {
    it("should throw for a missing session", () => {
      assert.throws(() => {
        instance.handleDiscoveryStreamLoadedContent("a_missing_port", {});
      }, "Session does not exist.");
    });
    it("should store loaded content to loadedContentSets", () => {
      const session = instance.addSession("new_session", "about:newtab");
      instance.handleDiscoveryStreamLoadedContent("new_session", {
        source: "foo",
        tiles: [{ id: 1, pos: 0 }],
      });

      assert.equal(Object.keys(session.loadedContentSets).length, 1);
      assert.deepEqual(session.loadedContentSets.foo, [{ id: 1, pos: 0 }]);

      // Add another ping with the same source
      instance.handleDiscoveryStreamLoadedContent("new_session", {
        source: "foo",
        tiles: [{ id: 2, pos: 1 }],
      });

      assert.deepEqual(session.loadedContentSets.foo, [
        { id: 1, pos: 0 },
        { id: 2, pos: 1 },
      ]);

      // Add another ping with a different source
      instance.handleDiscoveryStreamLoadedContent("new_session", {
        source: "bar",
        tiles: [{ id: 3, pos: 2 }],
      });

      assert.equal(Object.keys(session.loadedContentSets).length, 2);
      assert.deepEqual(session.loadedContentSets.foo, [
        { id: 1, pos: 0 },
        { id: 2, pos: 1 },
      ]);
      assert.deepEqual(session.loadedContentSets.bar, [{ id: 3, pos: 2 }]);
    });
  });
  describe("#_generateStructuredIngestionEndpoint", () => {
    it("should generate a valid endpoint", () => {
      const fakeEndpoint = "http://fakeendpoint.com/base/";
      const fakeUUID = "{34f24486-f01a-9749-9c5b-21476af1fa77}";
      const fakeUUIDWithoutBraces = fakeUUID.substring(1, fakeUUID.length - 1);
      FakePrefs.prototype.prefs = {};
      FakePrefs.prototype.prefs[
        STRUCTURED_INGESTION_ENDPOINT_PREF
      ] = fakeEndpoint;
      sandbox.stub(Services.uuid, "generateUUID").returns(fakeUUID);
      const feed = new TelemetryFeed();
      const url = feed._generateStructuredIngestionEndpoint(
        "testNameSpace",
        "testPingType",
        "1"
      );

      assert.equal(
        url,
        `${fakeEndpoint}/testNameSpace/testPingType/1/${fakeUUIDWithoutBraces}`
      );
    });
  });
  describe("#handleASRouterUserEvent", () => {
    it("should call sendStructuredIngestionEvent on known pingTypes", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "IMPRESSION",
        message_id: "12345",
      };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendStructuredIngestionEvent");

      await instance.handleASRouterUserEvent({ data });

      assert.calledOnce(instance.sendStructuredIngestionEvent);
    });
    it("should reportError on unknown pingTypes", async () => {
      const data = {
        action: "unknown_event",
        event: "IMPRESSION",
        message_id: "12345",
      };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendStructuredIngestionEvent");

      await instance.handleASRouterUserEvent({ data });

      assert.calledOnce(global.Cu.reportError);
      assert.notCalled(instance.sendStructuredIngestionEvent);
    });
  });
  describe("#isInCFRCohort", () => {
    it("should return false if there is no CFR experiment registered", () => {
      assert.ok(!instance.isInCFRCohort);
    });
    it("should return true if there is a CFR experiment registered", () => {
      sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
        slug: "SOME-CFR-EXP",
      });

      assert.ok(instance.isInCFRCohort);
      assert.propertyVal(
        ExperimentAPI.getExperimentMetaData.firstCall.args[0],
        "featureId",
        "cfr"
      );
    });
  });
  describe("#handleTopSitesImpressionStats", () => {
    it("should call sendStructuredIngestionEvent on an impression event", async () => {
      const data = {
        type: "impression",
        tile_id: 42,
        source: "newtab",
        position: 1,
        reporting_url: "https://test.reporting.net/",
      };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendStructuredIngestionEvent");
      sandbox.spy(Services.telemetry, "keyedScalarAdd");

      await instance.handleTopSitesImpressionStats({ data });

      // Scalar should be added
      assert.calledOnce(Services.telemetry.keyedScalarAdd);
      assert.calledWith(
        Services.telemetry.keyedScalarAdd,
        "contextual.services.topsites.impression",
        "newtab_1",
        1
      );

      assert.calledOnce(instance.sendStructuredIngestionEvent);

      const { args } = instance.sendStructuredIngestionEvent.firstCall;
      // payload
      assert.deepEqual(args[0], {
        context_id: FAKE_UUID,
        tile_id: 42,
        source: "newtab",
        position: 1,
        reporting_url: "https://test.reporting.net/",
      });
      // namespace
      assert.equal(args[1], "contextual-services");
      // docType
      assert.equal(args[2], "topsites-impression");
      // version
      assert.equal(args[3], "1");
    });
    it("should call sendStructuredIngestionEvent on a click event", async () => {
      const data = {
        type: "click",
        tile_id: 42,
        source: "newtab",
        position: 1,
        reporting_url: "https://test.reporting.net/",
      };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendStructuredIngestionEvent");
      sandbox.spy(Services.telemetry, "keyedScalarAdd");

      await instance.handleTopSitesImpressionStats({ data });

      // Scalar should be added
      assert.calledOnce(Services.telemetry.keyedScalarAdd);
      assert.calledWith(
        Services.telemetry.keyedScalarAdd,
        "contextual.services.topsites.click",
        "newtab_1",
        1
      );

      assert.calledOnce(instance.sendStructuredIngestionEvent);

      const { args } = instance.sendStructuredIngestionEvent.firstCall;
      // payload
      assert.deepEqual(args[0], {
        context_id: FAKE_UUID,
        tile_id: 42,
        source: "newtab",
        position: 1,
        reporting_url: "https://test.reporting.net/",
      });
      // namespace
      assert.equal(args[1], "contextual-services");
      // docType
      assert.equal(args[2], "topsites-click");
      // version
      assert.equal(args[3], "1");
    });
    it("should reportError on unknown pingTypes", async () => {
      const data = { type: "unknown_type" };
      instance = new TelemetryFeed();
      sandbox.spy(instance, "sendStructuredIngestionEvent");

      await instance.handleTopSitesImpressionStats({ data });

      assert.calledOnce(global.Cu.reportError);
      assert.notCalled(instance.sendStructuredIngestionEvent);
    });
  });
});
