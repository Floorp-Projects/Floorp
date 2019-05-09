/* global Services */
import {actionCreators as ac, actionTypes as at, actionUtils as au} from "common/Actions.jsm";
import {
  ASRouterEventPing,
  BasePing,
  ImpressionStatsPing,
  PerfPing,
  SessionPing,
  SpocsFillPing,
  UndesiredPing,
  UserEventPing,
} from "test/schemas/pings";
import {FakePrefs, GlobalOverrider} from "test/unit/utils";
import {ASRouterPreferences} from "lib/ASRouterPreferences.jsm";
import injector from "inject!lib/TelemetryFeed.jsm";

const FAKE_UUID = "{foo-123-foo}";
const FAKE_ROUTER_MESSAGE_PROVIDER = [{id: "cfr", enabled: true}];
const FAKE_ROUTER_MESSAGE_PROVIDER_COHORT = [{id: "cfr", enabled: true, cohort: "cohort_group"}];

describe("TelemetryFeed", () => {
  let globals;
  let sandbox;
  let expectedUserPrefs;
  let browser = {getAttribute() { return "true"; }};
  let instance;
  let clock;
  let fakeHomePageUrl;
  let fakeHomePage;
  let fakeExtensionSettingsStore;
  class PingCentre {sendPing() {} uninit() {} sendStructuredIngestionPing() {}}
  class UTEventReporting {
    sendUserEvent() {}
    sendSessionEndEvent() {}
    sendTrailheadEnrollEvent() {}
    uninit() {}
  }
  class PerfService {
    getMostRecentAbsMarkStartByName() { return 1234; }
    mark() {}
    absNow() { return 123; }
    get timeOrigin() { return 123456; }
  }
  const perfService = new PerfService();
  const {
    TelemetryFeed,
    USER_PREFS_ENCODING,
    PREF_IMPRESSION_ID,
    TELEMETRY_PREF,
    EVENTS_TELEMETRY_PREF,
    STRUCTURED_INGESTION_TELEMETRY_PREF,
    STRUCTURED_INGESTION_ENDPOINT_PREF,
  } = injector({
    "common/PerfService.jsm": {perfService},
    "lib/UTEventReporting.jsm": {UTEventReporting},
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
    globals.set("gUUIDGenerator", {generateUUID: () => FAKE_UUID});
    globals.set("aboutNewTabService", {
      overridden: false,
      newTabURL: "",
    });
    globals.set("HomePage", fakeHomePage);
    globals.set("ExtensionSettingsStore", fakeExtensionSettingsStore);
    globals.set("PingCentre", PingCentre);
    globals.set("UTEventReporting", UTEventReporting);
    sandbox.stub(ASRouterPreferences, "providers").get(() => FAKE_ROUTER_MESSAGE_PROVIDER);
    instance = new TelemetryFeed();
  });
  afterEach(() => {
    clock.restore();
    globals.restore();
    FakePrefs.prototype.prefs = {};
    ASRouterPreferences.uninit();
  });
  describe("#init", () => {
    it("should add .pingCentre, a PingCentre instance", () => {
      assert.instanceOf(instance.pingCentre, PingCentre);
    });
    it("should add .pingCentreForASRouter, a PingCentre instance", () => {
      assert.instanceOf(instance.pingCentreForASRouter, PingCentre);
    });
    it("should add .utEvents, a UTEventReporting instance", () => {
      assert.instanceOf(instance.utEvents, UTEventReporting);
    });
    it("should make this.browserOpenNewtabStart() observe browser-open-newtab-start", () => {
      sandbox.spy(Services.obs, "addObserver");

      instance.init();

      assert.calledTwice(Services.obs.addObserver);
      assert.calledWithExactly(Services.obs.addObserver,
        instance.browserOpenNewtabStart, "browser-open-newtab-start");
    });
    it("should add window open listener", () => {
      sandbox.spy(Services.obs, "addObserver");

      instance.init();

      assert.calledTwice(Services.obs.addObserver);
      assert.calledWithExactly(Services.obs.addObserver,
        instance._addWindowListeners, "domwindowopened");
    });
    it("should add TabPinned event listener on new windows", () => {
      const stub = {addEventListener: sandbox.stub()};
      sandbox.spy(Services.obs, "addObserver");

      instance.init();

      assert.calledTwice(Services.obs.addObserver);
      const [cb] = Services.obs.addObserver.secondCall.args;
      cb(stub);
      assert.calledTwice(stub.addEventListener);
      assert.calledWithExactly(stub.addEventListener, "unload", instance.handleEvent);
      assert.calledWithExactly(stub.addEventListener, "TabPinned", instance.handleEvent);
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
          wm: {getEnumerator: () => [{addEventListener: stub}]},
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
    describe("Structured Ingestion telemetry pref changes from false to true", () => {
      beforeEach(() => {
        FakePrefs.prototype.prefs = {};
        FakePrefs.prototype.prefs[STRUCTURED_INGESTION_TELEMETRY_PREF] = false;
        instance = new TelemetryFeed();

        assert.propertyVal(instance, "structuredIngestionTelemetryEnabled", false);
      });

      it("should set the enabled property to true", () => {
        instance._prefs.set(STRUCTURED_INGESTION_TELEMETRY_PREF, true);

        assert.propertyVal(instance, "structuredIngestionTelemetryEnabled", true);
      });
    });
  });
  describe("#handleEvent", () => {
    it("should dispatch a TAB_PINNED_EVENT", () => {
      sandbox.stub(instance, "sendEvent");
      globals.set({
        Services: {
          ...Services,
          wm: {getEnumerator: () => [{gBrowser: {tabs: [{pinned: true}]}}]},
        },
      });

      instance.handleEvent({type: "TabPinned", target: {}});

      assert.calledOnce(instance.sendEvent);
      const [ping] = instance.sendEvent.firstCall.args;
      assert.propertyVal(ping, "event", "TABPINNED");
      assert.propertyVal(ping, "source", "TAB_CONTEXT_MENU");
      assert.propertyVal(ping, "session_id", "n/a");
      assert.propertyVal(ping.value, "total_pinned_tabs", 1);
    });
    it("should skip private windows", () => {
      sandbox.stub(instance, "sendEvent");
      globals.set({PrivateBrowsingUtils: {isWindowPrivate: () => true}});

      instance.handleEvent({type: "TabPinned", target: {}});

      assert.notCalled(instance.sendEvent);
    });
    it("should return the correct value for total_pinned_tabs", () => {
      sandbox.stub(instance, "sendEvent");
      globals.set({
        Services: {
          ...Services,
          wm: {
            getEnumerator: () => [{
              gBrowser: {tabs: [{pinned: true}, {pinned: false}]},
            }],
          },
        },
      });

      instance.handleEvent({type: "TabPinned", target: {}});

      assert.calledOnce(instance.sendEvent);
      const [ping] = instance.sendEvent.firstCall.args;
      assert.propertyVal(ping, "event", "TABPINNED");
      assert.propertyVal(ping, "source", "TAB_CONTEXT_MENU");
      assert.propertyVal(ping, "session_id", "n/a");
      assert.propertyVal(ping.value, "total_pinned_tabs", 1);
    });
    it("should return the correct value for total_pinned_tabs (when private windows are open)", () => {
      sandbox.stub(instance, "sendEvent");
      const privateWinStub = sandbox.stub().onCall(0).returns(false)
        .onCall(1)
        .returns(true);
      globals.set({PrivateBrowsingUtils: {isWindowPrivate: privateWinStub}});
      globals.set({
        Services: {
          ...Services,
          wm: {
            getEnumerator: () => [{
              gBrowser: {tabs: [{pinned: true}, {pinned: true}]},
            }],
          },
        },
      });

      instance.handleEvent({type: "TabPinned", target: {}});

      assert.calledOnce(instance.sendEvent);
      const [ping] = instance.sendEvent.firstCall.args;
      assert.propertyVal(ping.value, "total_pinned_tabs", 0);
    });
    it("should unregister the event listeners", () => {
      const stub = {removeEventListener: sandbox.stub()};

      instance.handleEvent({type: "unload", target: stub});

      assert.calledTwice(stub.removeEventListener);
      assert.calledWithExactly(stub.removeEventListener, "unload", instance.handleEvent);
      assert.calledWithExactly(stub.removeEventListener, "TabPinned", instance.handleEvent);
    });
  });
  describe("#addSession", () => {
    it("should add a session and return it", () => {
      const session = instance.addSession("foo");

      assert.equal(instance.sessions.get("foo"), session);
    });
    it("should set the session_id", () => {
      sandbox.spy(global.gUUIDGenerator, "generateUUID");

      const session = instance.addSession("foo");

      assert.calledOnce(global.gUUIDGenerator.generateUUID);
      assert.equal(session.session_id, global.gUUIDGenerator.generateUUID.firstCall.returnValue);
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

      assert.propertyVal(session.perf, "load_trigger_type",
        "first_window_opened");
    });
    it("should not set load_trigger_type to first_window_opened on the second about:home seen", () => {
      instance.addSession("foo", "about:home");

      const session2 = instance.addSession("foo", "about:home");

      assert.notPropertyVal(session2.perf, "load_trigger_type",
        "first_window_opened");
    });
    it("should set load_trigger_ts to the value of perfService.timeOrigin", () => {
      const session = instance.addSession("foo", "about:home");

      assert.propertyVal(session.perf, "load_trigger_ts",
        123456);
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
      instance.saveSessionPerfData("foo", {topsites_data_late_by_ms: 10});
      instance.saveSessionPerfData("foo", {highlights_data_late_by_ms: 20});

      // Create a ping referencing the session
      const ping = instance.createSessionEndEvent(session);
      assert.validate(ping, SessionPing);
      assert.propertyVal(instance.sessions.get("foo").perf,
                         "highlights_data_late_by_ms", 20);
      assert.propertyVal(instance.sessions.get("foo").perf,
                         "topsites_data_late_by_ms", 10);
    });
    it("should be a valid ping with the topsites stats perf", () => {
      // Add a session
      const portID = "foo";
      const session = instance.addSession(portID, "about:home");
      instance.saveSessionPerfData("foo", {
        topsites_icon_stats: {
          "custom_screenshot": 0,
          "screenshot_with_icon": 2,
          "screenshot": 1,
          "tippytop": 2,
          "rich_icon": 1,
          "no_image": 0,
        },
        topsites_pinned: 3,
        topsites_search_shortcuts: 2,
      });

      // Create a ping referencing the session
      const ping = instance.createSessionEndEvent(session);
      assert.validate(ping, SessionPing);
      assert.propertyVal(instance.sessions.get("foo").perf.topsites_icon_stats,
        "screenshot_with_icon", 2);
      assert.equal(instance.sessions.get("foo").perf.topsites_pinned, 3);
      assert.equal(instance.sessions.get("foo").perf.topsites_search_shortcuts, 2);
    });
  });

  describe("#browserOpenNewtabStart", () => {
    it("should call perfService.mark with browser-open-newtab-start", () => {
      sandbox.stub(perfService, "mark");

      instance.browserOpenNewtabStart();

      assert.calledOnce(perfService.mark);
      assert.calledWithExactly(perfService.mark, "browser-open-newtab-start");
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
      assert.ok(Number.isInteger(session.session_duration),
        "session_duration should be an integer");
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

      let sessionEndEvent = instance.createSessionEndEvent.firstCall.returnValue;
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
        assert.propertyVal(instance.sessions.get("foo").perf, "load_trigger_type", "unexpected");
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
        const data = {source: "TOP_SITES", event: "CLICK"};
        const action = ac.AlsoToMain(ac.UserEvent(data), portID);
        const session = instance.addSession(portID);

        const ping = await instance.createUserEvent(action);

        // Is it valid?
        assert.validate(ping, UserEventPing);
        // Does it have the right session_id?
        assert.propertyVal(ping, "session_id", session.session_id);
      });
    });
    describe("#createUndesiredEvent", () => {
      it("should create a valid event without a session", async () => {
        const action = ac.UndesiredEvent({source: "TOP_SITES", event: "MISSING_IMAGE", value: 10});

        const ping = await instance.createUndesiredEvent(action);

        // Is it valid?
        assert.validate(ping, UndesiredPing);
        // Does it have the right value?
        assert.propertyVal(ping, "value", 10);
      });
      it("should create a valid event with a session", async () => {
        const portID = "foo";
        const data = {source: "TOP_SITES", event: "MISSING_IMAGE", value: 10};
        const action = ac.AlsoToMain(ac.UndesiredEvent(data), portID);
        const session = instance.addSession(portID);

        const ping = await instance.createUndesiredEvent(action);

        // Is it valid?
        assert.validate(ping, UndesiredPing);
        // Does it have the right session_id?
        assert.propertyVal(ping, "session_id", session.session_id);
        // Does it have the right value?
        assert.propertyVal(ping, "value", 10);
      });
      describe("#validate *_data_late_by_ms", () => {
        it("should create a valid highlights_data_late_by_ms ping", () => {
          const data = {
            type: at.TELEMETRY_UNDESIRED_EVENT,
            data: {
              source: "HIGHLIGHTS",
              event: `highlights_data_late_by_ms`,
              value: 2,
            },
          };
          const ping = instance.createUndesiredEvent(data);

          assert.validate(ping, UndesiredPing);
          assert.propertyVal(ping, "value", data.data.value);
          assert.propertyVal(ping, "event", data.data.event);
        });
      });
    });
    describe("#createPerformanceEvent", () => {
      it("should create a valid event without a session", async () => {
        const action = ac.PerfEvent({event: "SCREENSHOT_FINISHED", value: 100});
        const ping = await instance.createPerformanceEvent(action);

        // Is it valid?
        assert.validate(ping, PerfPing);
        // Does it have the right value?
        assert.propertyVal(ping, "value", 100);
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
            is_prerendered: true,
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
            is_prerendered: true,
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
      const tiles = [{id: 10001}, {id: 10002}, {id: 10003}];
      const action = ac.ImpressionStats({source: "POCKET", tiles});
      const ping = await instance.createImpressionStats(au.getPortIdOfSender(action), action.data);

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "source", "POCKET");
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should create a valid click ping", async () => {
      const tiles = [{id: 10001, pos: 2}];
      const action = ac.ImpressionStats({source: "POCKET", tiles, click: 0});
      const ping = await instance.createImpressionStats(au.getPortIdOfSender(action), action.data);

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "click", 0);
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should create a valid block ping", async () => {
      const tiles = [{id: 10001, pos: 2}];
      const action = ac.ImpressionStats({source: "POCKET", tiles, block: 0});
      const ping = await instance.createImpressionStats(au.getPortIdOfSender(action), action.data);

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "block", 0);
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should create a valid pocket ping", async () => {
      const tiles = [{id: 10001, pos: 2}];
      const action = ac.ImpressionStats({source: "POCKET", tiles, pocket: 0});
      const ping = await instance.createImpressionStats(au.getPortIdOfSender(action), action.data);

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "pocket", 0);
      assert.propertyVal(ping, "tiles", tiles);
    });
  });
  describe("#createSpocsFillPing", () => {
    it("should create a valid SPOCS Fill ping", async () => {
      const spocFills = [
        {id: 10001, displayed: 0, reason: "frequency_cap", full_recalc: 1},
        {id: 10002, displayed: 0, reason: "blocked_by_user", full_recalc: 1},
        {id: 10003, displayed: 1, reason: "n/a", full_recalc: 1},
      ];
      const action = ac.DiscoveryStreamSpocsFill({spoc_fills: spocFills});
      const ping = await instance.createSpocsFillPing(action.data);

      assert.validate(ping, SpocsFillPing);
      assert.propertyVal(ping, "spoc_fills", spocFills);
    });
  });
  describe("#applyCFRPolicy", () => {
    it("should use client_id and message_id in prerelease", () => {
      globals.set("UpdateUtils", {getUpdateChannel() { return "nightly"; }});
      const data = {
        action: "cfr_user_event",
        source: "CFR",
        event: "IMPRESSION",
        client_id: "some_client_id",
        impression_id: "some_impression_id",
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const ping = instance.applyCFRPolicy(data);

      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", "n/a");
      assert.propertyVal(ping, "message_id", "cfr_message_01");
      assert.isUndefined(ping.bucket_id);
    });
    it("should use impression_id and bucket_id in release", () => {
      globals.set("UpdateUtils", {getUpdateChannel() { return "release"; }});
      const data = {
        action: "cfr_user_event",
        source: "CFR",
        event: "IMPRESSION",
        client_id: "some_client_id",
        impression_id: "some_impression_id",
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const ping = instance.applyCFRPolicy(data);

      assert.propertyVal(ping, "impression_id", FAKE_UUID);
      assert.propertyVal(ping, "client_id", "n/a");
      assert.propertyVal(ping, "message_id", "cfr_bucket_01");
      assert.isUndefined(ping.bucket_id);
    });
    it("should use client_id and message_id in the experiment cohort in release", () => {
      globals.set("UpdateUtils", {getUpdateChannel() { return "release"; }});
      sandbox.stub(ASRouterPreferences, "providers").get(() => FAKE_ROUTER_MESSAGE_PROVIDER_COHORT);
      const data = {
        action: "cfr_user_event",
        source: "CFR",
        event: "IMPRESSION",
        client_id: "some_client_id",
        impression_id: "some_impression_id",
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const ping = instance.applyCFRPolicy(data);

      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", "n/a");
      assert.propertyVal(ping, "message_id", "cfr_message_01");
      assert.isUndefined(ping.bucket_id);
    });
  });
  describe("#applySnippetsPolicy", () => {
    it("should drop client_id and unset impression_id", () => {
      const data = {
        action: "snippets_user_event",
        source: "SNIPPETS",
        event: "IMPRESSION",
        client_id: "n/a",
        impression_id: "some_impression_id",
        message_id: "snippets_message_01",
      };
      const ping = instance.applySnippetsPolicy(data);

      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", "n/a");
      assert.propertyVal(ping, "message_id", "snippets_message_01");
    });
  });
  describe("#applyOnboardingPolicy", () => {
    it("should drop client_id and unset impression_id", () => {
      const data = {
        action: "onboarding_user_event",
        source: "ONBOARDING",
        event: "CLICK_BUTTION",
        client_id: "n/a",
        impression_id: "some_impression_id",
        message_id: "onboarding_message_01",
      };
      const ping = instance.applyOnboardingPolicy(data);

      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", "n/a");
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
    });
  });
  describe("#createASRouterEvent", () => {
    it("should create a valid AS Router event", async () => {
      const data = {
        action: "snippet_user_event",
        source: "SNIPPETS",
        event: "CLICK",
        message_id: "snippets_message_01",
      };
      const action = ac.ASRouterUserEvent(data);
      const ping = await instance.createASRouterEvent(action);

      assert.validate(ping, ASRouterEventPing);
      assert.propertyVal(ping, "client_id", "n/a");
      assert.propertyVal(ping, "source", "SNIPPETS");
      assert.propertyVal(ping, "event", "CLICK");
    });
    it("should call applyCFRPolicy if action equals to cfr_user_event", async () => {
      const data = {
        action: "cfr_user_event",
        source: "CFR",
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
        source: "SNIPPETS",
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
        source: "ONBOARDING",
        event: "CLICK_BUTTON",
        message_id: "onboarding_message_01",
      };
      sandbox.stub(instance, "applyOnboardingPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyOnboardingPolicy);
    });
  });
  describe("#sendEvent", () => {
    it("should call PingCentre", async () => {
      FakePrefs.prototype.prefs.telemetry = true;
      const event = {};
      instance = new TelemetryFeed();
      sandbox.stub(instance.pingCentre, "sendPing");

      await instance.sendEvent(event);

      assert.calledWith(instance.pingCentre.sendPing, event);
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
      FakePrefs.prototype.prefs[STRUCTURED_INGESTION_TELEMETRY_PREF] = true;
      const event = {};
      instance = new TelemetryFeed();
      sandbox.stub(instance.pingCentre, "sendStructuredIngestionPing");

      await instance.sendStructuredIngestionEvent(event, "http://foo.com/base/");

      assert.calledWith(instance.pingCentre.sendStructuredIngestionPing, event);
    });
  });
  describe("#sendASRouterEvent", () => {
    it("should call PingCentre for AS Router", async () => {
      FakePrefs.prototype.prefs.telemetry = true;
      const event = {};
      instance = new TelemetryFeed();
      sandbox.stub(instance.pingCentreForASRouter, "sendPing");

      instance.sendASRouterEvent(event);

      assert.calledWith(instance.pingCentreForASRouter.sendPing, event);
    });
  });

  describe("#setLoadTriggerInfo", () => {
    it("should call saveSessionPerfData w/load_trigger_{ts,type} data", () => {
      const stub = sandbox.stub(instance, "saveSessionPerfData");
      sandbox.stub(perfService, "getMostRecentAbsMarkStartByName").returns(777);
      instance.addSession("port123");

      instance.setLoadTriggerInfo("port123");

      assert.calledWith(stub, "port123", {
        load_trigger_ts: 777,
        load_trigger_type: "menu_plus_or_keyboard",
      });
    });

    it("should not call saveSessionPerfData when getting mark throws", () => {
      const stub = sandbox.stub(instance, "saveSessionPerfData");
      sandbox.stub(perfService, "getMostRecentAbsMarkStartByName").throws();
      instance.addSession("port123");

      instance.setLoadTriggerInfo("port123");

      assert.notCalled(stub);
    });
  });

  describe("#saveSessionPerfData", () => {
    it("should update the given session with the given data", () => {
      instance.addSession("port123");
      assert.notProperty(instance.sessions.get("port123"), "fake_ts");
      const data = {fake_ts: 456, other_fake_ts: 789};

      instance.saveSessionPerfData("port123", data);

      assert.include(instance.sessions.get("port123").perf, data);
    });

    it("should call setLoadTriggerInfo if data has visibility_event_rcvd_ts", () => {
      sandbox.stub(instance, "setLoadTriggerInfo");
      instance.addSession("port123");
      const data = {visibility_event_rcvd_ts: 444455};

      instance.saveSessionPerfData("port123", data);

      assert.calledOnce(instance.setLoadTriggerInfo);
      assert.calledWithExactly(instance.setLoadTriggerInfo, "port123");
      assert.include(instance.sessions.get("port123").perf, data);
    });

    it("shouldn't call setLoadTriggerInfo if data has no visibility_event_rcvd_ts", () => {
      sandbox.stub(instance, "setLoadTriggerInfo");
      instance.addSession("port123");

      instance.saveSessionPerfData("port123", {monkeys_ts: 444455});

      assert.notCalled(instance.setLoadTriggerInfo);
    });

    it("should not call setLoadTriggerInfo when url is about:home", () => {
      sandbox.stub(instance, "setLoadTriggerInfo");
      instance.addSession("port123", "about:home");
      const data = {visibility_event_rcvd_ts: 444455};

      instance.saveSessionPerfData("port123", data);

      assert.notCalled(instance.setLoadTriggerInfo);
    });

    it("should call maybeRecordTopsitesPainted when url is about:home and topsites_first_painted_ts is given", () => {
      const topsites_first_painted_ts = 44455;
      const data = {topsites_first_painted_ts};
      const spy = sandbox.spy();

      sandbox.stub(Services.prefs, "getIntPref").returns(1);
      globals.set("aboutNewTabService", {
        overridden: false,
        newTabURL: "",
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
    it("should call .pingCentreForASRouter.uninit", () => {
      const stub = sandbox.stub(instance.pingCentreForASRouter, "uninit");

      instance.uninit();

      assert.calledOnce(stub);
    });
    it("should make this.browserOpenNewtabStart() stop observing browser-open-newtab-start and domwindowopened", async () => {
      await instance.init();
      sandbox.spy(Services.obs, "removeObserver");
      sandbox.stub(instance.pingCentre, "uninit");

      await instance.uninit();

      assert.calledTwice(Services.obs.removeObserver);
      assert.calledWithExactly(Services.obs.removeObserver,
        instance.browserOpenNewtabStart, "browser-open-newtab-start");
      assert.calledWithExactly(Services.obs.removeObserver,
        instance._addWindowListeners, "domwindowopened");
    });
  });
  describe("#onAction", () => {
    beforeEach(() => {
      FakePrefs.prototype.prefs = {};
    });
    it("should call .init() on an INIT action", () => {
      const init = sandbox.stub(instance, "init");
      const sendPageTakeoverData = sandbox.stub(instance, "sendPageTakeoverData");

      instance.onAction({type: at.INIT});

      assert.calledOnce(init);
      assert.calledOnce(sendPageTakeoverData);
    });
    it("should call .uninit() on an UNINIT action", () => {
      const stub = sandbox.stub(instance, "uninit");

      instance.onAction({type: at.UNINIT});

      assert.calledOnce(stub);
    });
    it("should call .handleNewTabInit on a NEW_TAB_INIT action", () => {
      sandbox.spy(instance, "handleNewTabInit");

      instance.onAction(ac.AlsoToMain({
        type: at.NEW_TAB_INIT,
        data: {url: "about:newtab", browser},
      }));

      assert.calledOnce(instance.handleNewTabInit);
    });
    it("should call .addSession() on a NEW_TAB_INIT action", () => {
      const stub = sandbox.stub(instance, "addSession").returns({perf: {}});
      sandbox.stub(instance, "setLoadTriggerInfo");

      instance.onAction(ac.AlsoToMain({
        type: at.NEW_TAB_INIT,
        data: {url: "about:monkeys", browser},
      }, "port123"));

      assert.calledOnce(stub);
      assert.calledWith(stub, "port123", "about:monkeys");
    });
    it("should call .endSession() on a NEW_TAB_UNLOAD action", () => {
      const stub = sandbox.stub(instance, "endSession");

      instance.onAction(ac.AlsoToMain({type: at.NEW_TAB_UNLOAD}, "port123"));

      assert.calledWith(stub, "port123");
    });
    it("should call .saveSessionPerfData on SAVE_SESSION_PERF_DATA", () => {
      const stub = sandbox.stub(instance, "saveSessionPerfData");
      const data = {some_ts: 10};
      const action = {type: at.SAVE_SESSION_PERF_DATA, data};

      instance.onAction(ac.AlsoToMain(action, "port123"));

      assert.calledWith(stub, "port123", data);
    });
    it("should send an event on a TELEMETRY_UNDESIRED_EVENT action", () => {
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const eventCreator = sandbox.stub(instance, "createUndesiredEvent");
      const action = {type: at.TELEMETRY_UNDESIRED_EVENT};

      instance.onAction(action);

      assert.calledWith(eventCreator, action);
      assert.calledWith(sendEvent, eventCreator.returnValue);
    });
    it("should send an event on a TELEMETRY_USER_EVENT action", () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      FakePrefs.prototype.prefs[EVENTS_TELEMETRY_PREF] = true;
      instance = new TelemetryFeed();

      const sendEvent = sandbox.stub(instance, "sendEvent");
      const utSendUserEvent = sandbox.stub(instance.utEvents, "sendUserEvent");
      const eventCreator = sandbox.stub(instance, "createUserEvent");
      const action = {type: at.TELEMETRY_USER_EVENT};

      instance.onAction(action);

      assert.calledWith(eventCreator, action);
      assert.calledWith(sendEvent, eventCreator.returnValue);
      assert.calledWith(utSendUserEvent, eventCreator.returnValue);
    });
    it("should call handleASRouterUserEvent on TELEMETRY_USER_EVENT action", () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      FakePrefs.prototype.prefs[EVENTS_TELEMETRY_PREF] = true;
      instance = new TelemetryFeed();

      const eventHandler = sandbox.spy(instance, "handleASRouterUserEvent");
      const action = {type: at.AS_ROUTER_TELEMETRY_USER_EVENT, data: {event: "CLICK"}};

      instance.onAction(action);

      assert.calledWith(eventHandler, action);
    });
    it("should send an event on a TELEMETRY_PERFORMANCE_EVENT action", () => {
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const eventCreator = sandbox.stub(instance, "createPerformanceEvent");
      const action = {type: at.TELEMETRY_PERFORMANCE_EVENT};

      instance.onAction(action);

      assert.calledWith(eventCreator, action);
      assert.calledWith(sendEvent, eventCreator.returnValue);
    });
    it("should send an event on a TELEMETRY_IMPRESSION_STATS action", () => {
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const eventCreator = sandbox.stub(instance, "createImpressionStats");
      const tiles = [{id: 10001}, {id: 10002}, {id: 10003}];
      const action = ac.ImpressionStats({source: "POCKET", tiles});

      instance.onAction(action);

      assert.calledWith(eventCreator, au.getPortIdOfSender(action), action.data);
      assert.calledWith(sendEvent, eventCreator.returnValue);
    });
    it("should call .handlePagePrerendered on a PAGE_PRERENDERED action", () => {
      const session = {perf: {}};
      sandbox.stub(instance.sessions, "get").returns(session);
      sandbox.spy(instance, "handlePagePrerendered");

      instance.onAction(ac.AlsoToMain({type: at.PAGE_PRERENDERED}));

      assert.calledOnce(instance.handlePagePrerendered);
      assert.ok(session.perf.is_prerendered);
    });
    it("should call .handleDiscoveryStreamImpressionStats on a DISCOVERY_STREAM_IMPRESSION_STATS action", () => {
      const session = {};
      sandbox.stub(instance.sessions, "get").returns(session);
      const data = {source: "foo", tiles: [{id: 1}]};
      const action = {type: at.DISCOVERY_STREAM_IMPRESSION_STATS, data};
      sandbox.spy(instance, "handleDiscoveryStreamImpressionStats");

      instance.onAction(ac.AlsoToMain(action, "port123"));

      assert.calledWith(instance.handleDiscoveryStreamImpressionStats, "port123", data);
    });
    it("should call .handleDiscoveryStreamLoadedContent on a DISCOVERY_STREAM_LOADED_CONTENT action", () => {
      const session = {};
      sandbox.stub(instance.sessions, "get").returns(session);
      const data = {source: "foo", tiles: [{id: 1}]};
      const action = {type: at.DISCOVERY_STREAM_LOADED_CONTENT, data};
      sandbox.spy(instance, "handleDiscoveryStreamLoadedContent");

      instance.onAction(ac.AlsoToMain(action, "port123"));

      assert.calledWith(instance.handleDiscoveryStreamLoadedContent, "port123", data);
    });
    it("should send an event on a DISCOVERY_STREAM_SPOCS_FILL action", () => {
      const sendEvent = sandbox.stub(instance, "sendStructuredIngestionEvent");
      const eventCreator = sandbox.stub(instance, "createSpocsFillPing");
      const spocFills = [
        {id: 10001, displayed: 0, reason: "frequency_cap", full_recalc: 1},
        {id: 10002, displayed: 0, reason: "blocked_by_user", full_recalc: 1},
        {id: 10003, displayed: 1, reason: "n/a", full_recalc: 1},
      ];
      const action = ac.DiscoveryStreamSpocsFill({spoc_fills: spocFills});

      instance.onAction(action);

      assert.calledWith(eventCreator, action.data);
      assert.calledWith(sendEvent, eventCreator.returnValue);
    });
    it("should call .handleTrailheadEnrollEvent on a TRAILHEAD_ENROLL_EVENT action", () => {
      const data = {experiment: "foo", type: "bar", branch: "baz"};
      const action = {type: at.TRAILHEAD_ENROLL_EVENT, data};
      sandbox.spy(instance, "handleTrailheadEnrollEvent");

      instance.onAction(action);

      assert.calledWith(instance.handleTrailheadEnrollEvent, action);
    });
  });
  describe("#handlePagePrerendered", () => {
    it("should not throw if there is no session for the given port ID", () => {
      assert.doesNotThrow(() => instance.handlePagePrerendered("doesn't exist"));
    });
    it("should set the session as prerendered on a PAGE_PRERENDERED action", () => {
      const session = {perf: {}};
      sandbox.stub(instance.sessions, "get").returns(session);

      instance.onAction(ac.AlsoToMain({type: at.PAGE_PRERENDERED}));

      assert.ok(session.perf.is_prerendered);
    });
  });
  describe("#handleNewTabInit", () => {
    it("should set the session as preloaded if the browser is preloaded", () => {
      const session = {perf: {}};
      let preloadedBrowser = {getAttribute() { return "preloaded"; }};
      sandbox.stub(instance, "addSession").returns(session);

      instance.onAction(ac.AlsoToMain({
        type: at.NEW_TAB_INIT,
        data: {url: "about:newtab", browser: preloadedBrowser},
      }));

      assert.ok(session.perf.is_preloaded);
    });
    it("should set the session as non-preloaded if the browser is non-preloaded", () => {
      const session = {perf: {}};
      let nonPreloadedBrowser = {getAttribute() { return ""; }};
      sandbox.stub(instance, "addSession").returns(session);

      instance.onAction(ac.AlsoToMain({
        type: at.NEW_TAB_INIT,
        data: {url: "about:newtab", browser: nonPreloadedBrowser},
      }));

      assert.ok(!session.perf.is_preloaded);
    });
  });
  describe("#sendPageTakeoverData", () => {
    let fakePrefs = {"browser.newtabpage.enabled": true};

    beforeEach(() => {
      globals.set("Services", Object.assign({}, Services, {prefs: {getBoolPref: key => fakePrefs[key]}}));
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
      globals.set("aboutNewTabService", {
        overridden: true,
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
      fakeExtensionSettingsStore.getSetting = () => ({id: ID});
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
      const spy = sandbox.spy(instance, "sendEvent");
      const session = {
        impressionSets: {
          source_foo: [{id: 1, pos: 0}, {id: 2, pos: 1}],
          source_bar: [{id: 3, pos: 0}, {id: 4, pos: 1}],
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
      const spy = sandbox.spy(instance, "sendEvent");
      const session = {
        loadedContentSets: {
          source_foo: [{id: 1, pos: 0}, {id: 2, pos: 1}],
          source_bar: [{id: 3, pos: 0}, {id: 4, pos: 1}],
        },
      };
      instance.sendDiscoveryStreamLoadedContent("foo", session);

      assert.calledTwice(spy);

      let [payload] = spy.firstCall.args;
      let sources = new Set([]);
      sources.add(payload.source);
      assert.equal(payload.loaded, 2);
      assert.deepEqual(payload.tiles, session.loadedContentSets[payload.source]);

      [payload] = spy.secondCall.args;
      sources.add(payload.source);
      assert.equal(payload.loaded, 2);
      assert.deepEqual(payload.tiles, session.loadedContentSets[payload.source]);

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
      instance.handleDiscoveryStreamImpressionStats("new_session",
        {source: "foo", tiles: [{id: 1, pos: 0}]});

      assert.equal(Object.keys(session.impressionSets).length, 1);
      assert.deepEqual(session.impressionSets.foo, [{id: 1, pos: 0}]);

      // Add another ping with the same source
      instance.handleDiscoveryStreamImpressionStats("new_session",
        {source: "foo", tiles: [{id: 2, pos: 1}]});

      assert.deepEqual(session.impressionSets.foo,
        [{id: 1, pos: 0}, {id: 2, pos: 1}]);

      // Add another ping with a different source
      instance.handleDiscoveryStreamImpressionStats("new_session",
        {source: "bar", tiles: [{id: 3, pos: 2}]});

      assert.equal(Object.keys(session.impressionSets).length, 2);
      assert.deepEqual(session.impressionSets.foo, [{id: 1, pos: 0}, {id: 2, pos: 1}]);
      assert.deepEqual(session.impressionSets.bar, [{id: 3, pos: 2}]);
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
      instance.handleDiscoveryStreamLoadedContent("new_session",
        {source: "foo", tiles: [{id: 1, pos: 0}]});

      assert.equal(Object.keys(session.loadedContentSets).length, 1);
      assert.deepEqual(session.loadedContentSets.foo, [{id: 1, pos: 0}]);

      // Add another ping with the same source
      instance.handleDiscoveryStreamLoadedContent("new_session",
        {source: "foo", tiles: [{id: 2, pos: 1}]});

      assert.deepEqual(session.loadedContentSets.foo,
        [{id: 1, pos: 0}, {id: 2, pos: 1}]);

      // Add another ping with a different source
      instance.handleDiscoveryStreamLoadedContent("new_session",
        {source: "bar", tiles: [{id: 3, pos: 2}]});

      assert.equal(Object.keys(session.loadedContentSets).length, 2);
      assert.deepEqual(session.loadedContentSets.foo, [{id: 1, pos: 0}, {id: 2, pos: 1}]);
      assert.deepEqual(session.loadedContentSets.bar, [{id: 3, pos: 2}]);
    });
  });
  describe("#_generateStructuredIngestionEndpoint", () => {
    it("should generate a valid endpoint", () => {
      const fakeEndpoint = "http://fakeendpoint.com/base/";
      const fakeUUID = "{34f24486-f01a-9749-9c5b-21476af1fa77}";
      const fakeUUIDWithoutBraces = fakeUUID.substring(1, fakeUUID.length - 1);
      FakePrefs.prototype.prefs = {};
      FakePrefs.prototype.prefs[STRUCTURED_INGESTION_ENDPOINT_PREF] = fakeEndpoint;
      sandbox.stub(global.gUUIDGenerator, "generateUUID").returns(fakeUUID);
      const feed = new TelemetryFeed();
      const url = feed._generateStructuredIngestionEndpoint("testPingType", "1");

      assert.equal(url, `${fakeEndpoint}/testPingType/1/${fakeUUIDWithoutBraces}`);
    });
  });
  describe("#handleTrailheadEnrollEvent", () => {
    it("should send a TRAILHEAD_ENROLL_EVENT if the telemetry is enabled", () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      const data = {experiment: "foo", type: "bar", branch: "baz"};
      instance = new TelemetryFeed();
      sandbox.stub(instance.utEvents, "sendTrailheadEnrollEvent");

      instance.handleTrailheadEnrollEvent({data});

      assert.calledWith(instance.utEvents.sendTrailheadEnrollEvent, data);
    });
    it("should not send TRAILHEAD_ENROLL_EVENT if the telemetry is disabled", () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = false;
      const data = {experiment: "foo", type: "bar", branch: "baz"};
      instance = new TelemetryFeed();
      sandbox.stub(instance.utEvents, "sendTrailheadEnrollEvent");

      instance.handleTrailheadEnrollEvent({data});

      assert.notCalled(instance.utEvents.sendTrailheadEnrollEvent);
    });
  });
});
