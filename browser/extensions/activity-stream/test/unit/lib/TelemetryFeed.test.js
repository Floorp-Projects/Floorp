/* global Services */

const injector = require("inject!lib/TelemetryFeed.jsm");
const {GlobalOverrider, FakePrefs} = require("test/unit/utils");
const {actionCreators: ac, actionTypes: at} = require("common/Actions.jsm");
const {
  BasePing,
  UndesiredPing,
  UserEventPing,
  ImpressionStatsPing,
  PerfPing,
  SessionPing
} = require("test/schemas/pings");

const FAKE_UUID = "{foo-123-foo}";

describe("TelemetryFeed", () => {
  let globals;
  let sandbox;
  let expectedUserPrefs;
  let store = {
    dispatch() {},
    getState() { return {App: {version: "1.0.0", locale: "en-US"}}; }
  };
  let instance;
  let clock;
  class PingCentre {sendPing() {} uninit() {}}
  class PerfService {
    getMostRecentAbsMarkStartByName() { return 1234; }
    mark() {}
    absNow() { return 123; }
  }
  const perfService = new PerfService();
  const {
    TelemetryFeed,
    USER_PREFS_ENCODING,
    IMPRESSION_STATS_RESET_TIME,
    TELEMETRY_PREF,
    PREF_IMPRESSION_STATS_CLICKED,
    PREF_IMPRESSION_STATS_BLOCKED,
    PREF_IMPRESSION_STATS_POCKETED
  } = injector({"common/PerfService.jsm": {perfService}});

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    clock = sinon.useFakeTimers();
    sandbox.spy(global.Components.utils, "reportError");
    globals.set("gUUIDGenerator", {generateUUID: () => FAKE_UUID});
    globals.set("PingCentre", PingCentre);
    instance = new TelemetryFeed();
    instance.store = store;
  });
  afterEach(() => {
    clock.restore();
    globals.restore();
    FakePrefs.prototype.prefs = {};
  });
  describe("#init", () => {
    it("should add .pingCentre, a PingCentre instance", () => {
      assert.instanceOf(instance.pingCentre, PingCentre);
    });
    it("should make this.browserOpenNewtabStart() observe browser-open-newtab-start", () => {
      sandbox.spy(Services.obs, "addObserver");

      instance.init();

      assert.calledOnce(Services.obs.addObserver);
      assert.calledWithExactly(Services.obs.addObserver,
        instance.browserOpenNewtabStart, "browser-open-newtab-start");
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
    it("should set the page", () => {
      const session = instance.addSession("foo");

      assert.equal(session.page, "about:newtab"); // This is hardcoded for now.
    });
    it("should set the perf type when lacking timestamp", () => {
      const session = instance.addSession("foo");

      assert.propertyVal(session.perf, "load_trigger_type", "unexpected");
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
    it("shouldn't add session_duration if there's no visibility_event_rcvd_ts", () => {
      sandbox.stub(instance, "sendEvent");
      const session = instance.addSession("foo");

      instance.endSession("foo");

      assert.notProperty(session, "session_duration");
    });
    it("should remove the session from .sessions", () => {
      sandbox.stub(instance, "sendEvent");
      instance.addSession("foo");

      instance.endSession("foo");

      assert.isFalse(instance.sessions.has("foo"));
    });
    it("should call createSessionSendEvent and sendEvent with the sesssion", () => {
      sandbox.stub(instance, "sendEvent");
      sandbox.stub(instance, "createSessionEndEvent");
      const session = instance.addSession("foo");

      instance.endSession("foo");

      // Did we call sendEvent with the result of createSessionEndEvent?
      assert.calledWith(instance.createSessionEndEvent, session);
      assert.calledWith(instance.sendEvent, instance.createSessionEndEvent.firstCall.returnValue);
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
      });
      it("should create a valid base ping with session info if a portID is supplied", async () => {
        // Add a session
        const portID = "foo";
        instance.addSession(portID);
        const sessionID = instance.sessions.get(portID).session_id;

        // Create a ping referencing the session
        const ping = await instance.createPing(portID);
        assert.validate(ping, BasePing);

        // Make sure we added the right session-related stuff to the ping
        assert.propertyVal(ping, "session_id", sessionID);
        assert.propertyVal(ping, "page", "about:newtab");
      });
      it("should create an unexpected base ping if no session yet portID is supplied", async () => {
        const ping = await instance.createPing("foo");

        assert.validate(ping, BasePing);
        assert.propertyVal(ping, "page", "about:newtab");
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
        const action = ac.SendToMain(ac.UserEvent(data), portID);
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
        const action = ac.SendToMain(ac.UndesiredEvent(data), portID);
        const session = instance.addSession(portID);

        const ping = await instance.createUndesiredEvent(action);

        // Is it valid?
        assert.validate(ping, UndesiredPing);
        // Does it have the right session_id?
        assert.propertyVal(ping, "session_id", session.session_id);
        // Does it have the right value?
        assert.propertyVal(ping, "value", 10);
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
            visibility_event_rcvd_ts: 20
          }
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
          perf: {load_trigger_type: "unexpected"}
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
      const ping = await instance.createImpressionStats(action);

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "source", "POCKET");
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should empty all the user specific IDs in the ping", async () => {
      const tiles = [{id: 10001, pos: 2}];
      const incognito = true;
      const action = ac.ImpressionStats({source: "POCKET", incognito, tiles, click: 0});
      const ping = await instance.createImpressionStats(action);

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "click", 0);
      assert.propertyVal(ping, "tiles", tiles);
      assert.propertyVal(ping, "client_id", "n/a");
      assert.propertyVal(ping, "session_id", "n/a");
      assert.isUndefined(ping.incognito);
    });
    it("should create a valid click ping", async () => {
      const tiles = [{id: 10001, pos: 2}];
      const action = ac.ImpressionStats({source: "POCKET", tiles, click: 0});
      const ping = await instance.createImpressionStats(action);

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "click", 0);
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should create a valid block ping", async () => {
      const tiles = [{id: 10001, pos: 2}];
      const action = ac.ImpressionStats({source: "POCKET", tiles, block: 0});
      const ping = await instance.createImpressionStats(action);

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "block", 0);
      assert.propertyVal(ping, "tiles", tiles);
    });
    it("should create a valid pocket ping", async () => {
      const tiles = [{id: 10001, pos: 2}];
      const action = ac.ImpressionStats({source: "POCKET", tiles, pocket: 0});
      const ping = await instance.createImpressionStats(action);

      assert.validate(ping, ImpressionStatsPing);
      assert.propertyVal(ping, "pocket", 0);
      assert.propertyVal(ping, "tiles", tiles);
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

  describe("#setLoadTriggerInfo", () => {
    it("should call saveSessionPerfData w/load_trigger_{ts,type} data", () => {
      const stub = sandbox.stub(instance, "saveSessionPerfData");
      sandbox.stub(perfService, "getMostRecentAbsMarkStartByName").returns(777);
      instance.addSession("port123");

      instance.setLoadTriggerInfo("port123");

      assert.calledWith(stub, "port123", {
        load_trigger_ts: 777,
        load_trigger_type: "menu_plus_or_keyboard"
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
  });

  describe("#uninit", () => {
    it("should call .pingCentre.uninit", () => {
      const stub = sandbox.stub(instance.pingCentre, "uninit");
      instance.uninit();
      assert.calledOnce(stub);
    });
    it("should remove the a-s telemetry pref listener", () => {
      FakePrefs.prototype.prefs[TELEMETRY_PREF] = true;
      instance = new TelemetryFeed();
      assert.property(instance._prefs.observers, TELEMETRY_PREF);

      instance.uninit();

      assert.notProperty(instance._prefs.observers, TELEMETRY_PREF);
    });
    it("should call Cu.reportError if this._prefs.ignore throws", () => {
      globals.sandbox.stub(FakePrefs.prototype, "ignore").throws("Some Error");
      instance = new TelemetryFeed();

      instance.uninit();

      assert.called(global.Components.utils.reportError);
    });
    it("should make this.browserOpenNewtabStart() stop observing browser-open-newtab-start", async () => {
      await instance.init();
      sandbox.spy(Services.obs, "removeObserver");
      sandbox.stub(instance.pingCentre, "uninit");

      await instance.uninit();

      assert.calledOnce(Services.obs.removeObserver);
      assert.calledWithExactly(Services.obs.removeObserver,
        instance.browserOpenNewtabStart, "browser-open-newtab-start");
    });
  });
  describe("#resetImpressionStats", () => {
    beforeEach(() => {
      FakePrefs.prototype.prefs = {};
      FakePrefs.prototype.prefs[PREF_IMPRESSION_STATS_CLICKED] = "[10000]";
      FakePrefs.prototype.prefs[PREF_IMPRESSION_STATS_BLOCKED] = "[10001]";
      FakePrefs.prototype.prefs[PREF_IMPRESSION_STATS_POCKETED] = "[10002]";
    });
    it("should reset all the GUID sets for impression stats", () => {
      const lastResetTime = instance._impressionStatsLastReset;
      // Haven't restored the clock yet, we have to manually tick the clock.
      clock.tick(IMPRESSION_STATS_RESET_TIME);
      instance.resetImpressionStats();
      for (const key of Object.keys(instance._impressionStats)) {
        assert.equal(instance._impressionStats[key].size, 0);
      }
      assert.isAbove(instance._impressionStatsLastReset, lastResetTime);
    });
  });
  describe("#onAction", () => {
    beforeEach(() => {
      FakePrefs.prototype.prefs = {};
      FakePrefs.prototype.prefs[PREF_IMPRESSION_STATS_CLICKED] = "[]";
      FakePrefs.prototype.prefs[PREF_IMPRESSION_STATS_BLOCKED] = "[]";
      FakePrefs.prototype.prefs[PREF_IMPRESSION_STATS_POCKETED] = "[]";
    });
    it("should call .init() on an INIT action", () => {
      const stub = sandbox.stub(instance, "init");
      instance.onAction({type: at.INIT});
      assert.calledOnce(stub);
    });
    it("should call .addSession() on a NEW_TAB_INIT action", () => {
      const stub = sandbox.stub(instance, "addSession");
      sandbox.stub(instance, "setLoadTriggerInfo");

      instance.onAction(ac.SendToMain({
        type: at.NEW_TAB_INIT,
        data: {}
      }, "port123"));

      assert.calledOnce(stub);
      assert.calledWith(stub, "port123");
    });
    it("should call .endSession() on a NEW_TAB_UNLOAD action", () => {
      const stub = sandbox.stub(instance, "endSession");
      instance.onAction(ac.SendToMain({type: at.NEW_TAB_UNLOAD}, "port123"));
      assert.calledWith(stub, "port123");
    });
    it("should call .saveSessionPerfData on SAVE_SESSION_PERF_DATA", () => {
      const stub = sandbox.stub(instance, "saveSessionPerfData");
      const data = {some_ts: 10};
      const action = {type: at.SAVE_SESSION_PERF_DATA, data};

      instance.onAction(ac.SendToMain(action, "port123"));

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
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const eventCreator = sandbox.stub(instance, "createUserEvent");
      const action = {type: at.TELEMETRY_USER_EVENT};
      instance.onAction(action);
      assert.calledWith(eventCreator, action);
      assert.calledWith(sendEvent, eventCreator.returnValue);
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
      const action = {type: at.TELEMETRY_IMPRESSION_STATS, data: {}};
      instance.onAction(action);
      assert.calledWith(eventCreator, action);
      assert.calledWith(sendEvent, eventCreator.returnValue);
    });
    it("should call .resetImpressionStats on a SYSTEM_TICK action", () => {
      const resetImpressionStats = sandbox.stub(instance, "resetImpressionStats");

      instance.onAction({type: at.SYSTEM_TICK});
      assert.notCalled(resetImpressionStats);

      clock.tick(IMPRESSION_STATS_RESET_TIME);
      instance.onAction({type: at.SYSTEM_TICK});
      assert.calledOnce(resetImpressionStats);
    });
    it("should not send two click pings for the same article", async () => {
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const tiles = [{id: 10001, pos: 2}];
      const data = {tiles, click: 0};
      const action = {type: at.TELEMETRY_IMPRESSION_STATS, data};

      instance.onAction(action);
      instance.onAction(action);
      assert.calledOnce(sendEvent);
    });
    it("should not send two block pings for the same article", async () => {
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const tiles = [{id: 10001, pos: 2}];
      const data = {tiles, block: 0};
      const action = {type: at.TELEMETRY_IMPRESSION_STATS, data};

      instance.onAction(action);
      instance.onAction(action);
      assert.calledOnce(sendEvent);
    });
    it("should not send two save to pocket pings for the same article", async () => {
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const tiles = [{id: 10001, pos: 2}];
      const data = {tiles, pocket: 0};
      const action = {type: at.TELEMETRY_IMPRESSION_STATS, data};

      instance.onAction(action);
      instance.onAction(action);
      assert.calledOnce(sendEvent);
    });
  });
});

describe("PersistentGuidSet", () => {
  const {PersistentGuidSet} = injector({});

  afterEach(() => {
    FakePrefs.prototype.prefs = {};
  });
  describe("#init", () => {
    it("should initialized empty", () => {
      let guidSet;

      guidSet = new PersistentGuidSet(new FakePrefs(), "test.guidSet");
      assert.equal(guidSet.size, 0);
      assert.deepEqual(guidSet.items(), []);
    });
    it("should initialized from pref", () => {
      let guidSet;

      FakePrefs.prototype.prefs = {"test.guidSet": JSON.stringify([10000])};
      guidSet = new PersistentGuidSet(new FakePrefs(), "test.guidSet");
      assert.equal(guidSet.size, 1);
      assert.isTrue(guidSet.has(10000));
      assert.deepEqual(guidSet.items(), [10000]);
    });
    it("should initialized empty with invalid pref", () => {
      let guidSet;

      FakePrefs.prototype.prefs = {"test.guidSet": 10000};
      guidSet = new PersistentGuidSet(new FakePrefs(), "test.guidSet");
      assert.equal(guidSet.size, 0);
      assert.deepEqual(guidSet.items(), []);
    });
  });
  describe("#save", () => {
    it("should save the new GUID", () => {
      let guidSet;
      let prefs = new FakePrefs();

      guidSet = new PersistentGuidSet(prefs, "test.guidSet");
      guidSet.save("10000");
      guidSet.save("10001");
      assert.equal(guidSet.size, 2);
      assert.deepEqual(guidSet.items(), ["10000", "10001"]);
      assert.equal(prefs.get("test.guidSet"), "[\"10000\",\"10001\"]");
    });
    it("should not save the same GUID twice", () => {
      let guidSet;
      let prefs = new FakePrefs();

      guidSet = new PersistentGuidSet(prefs, "test.guidSet");
      guidSet.save("10000");
      assert.isFalse(guidSet.save("10000"));
      assert.equal(guidSet.size, 1);
      assert.deepEqual(guidSet.items(), ["10000"]);
      assert.equal(prefs.get("test.guidSet"), "[\"10000\"]");
    });
  });
  describe("#clear", () => {
    it("should clear the GUID set", () => {
      let guidSet;

      guidSet = new PersistentGuidSet(new FakePrefs(), "test.guidSet");
      guidSet.save("10000");
      guidSet.save("10001");
      assert.equal(guidSet.size, 2);
      guidSet.clear();
      assert.equal(guidSet.size, 0);
      assert.deepEqual(guidSet.items(), []);
    });
  });
});
