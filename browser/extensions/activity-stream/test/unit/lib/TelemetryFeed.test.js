/* global Services */

const injector = require("inject!lib/TelemetryFeed.jsm");
const {GlobalOverrider} = require("test/unit/utils");
const {actionCreators: ac, actionTypes: at} = require("common/Actions.jsm");
const {
  BasePing,
  UndesiredPing,
  UserEventPing,
  PerfPing,
  SessionPing
} = require("test/schemas/pings");

const FAKE_TELEMETRY_ID = "foo123";
const FAKE_UUID = "{foo-123-foo}";

describe("TelemetryFeed", () => {
  let globals;
  let sandbox;
  let store = {
    dispatch() {},
    getState() { return {App: {version: "1.0.0", locale: "en-US"}}; }
  };
  let instance;
  class TelemetrySender {sendPing() {} uninit() {}}
  class PerfService {
    getMostRecentAbsMarkStartByName() { return 1234; }
    mark() {}
    absNow() { return 123; }
  }
  const perfService = new PerfService();
  const {TelemetryFeed} = injector({
    "lib/TelemetrySender.jsm": {TelemetrySender},
    "common/PerfService.jsm": {perfService}
  });

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    globals.set("ClientID", {getClientID: sandbox.spy(async () => FAKE_TELEMETRY_ID)});
    globals.set("gUUIDGenerator", {generateUUID: () => FAKE_UUID});
    instance = new TelemetryFeed();
    instance.store = store;
  });
  afterEach(() => {
    globals.restore();
  });
  describe("#init", () => {
    it("should add .telemetrySender, a TelemetrySender instance", () => {
      assert.instanceOf(instance.telemetrySender, TelemetrySender);
    });
    it("should add .telemetryClientId from the ClientID module", async () => {
      assert.equal(await instance.telemetryClientId, FAKE_TELEMETRY_ID);
    });
    it("should make this.browserOpenNewtabStart() observe browser-open-newtab-start", () => {
      sandbox.spy(Services.obs, "addObserver");

      instance.init();

      assert.calledOnce(Services.obs.addObserver);
      assert.calledWithExactly(Services.obs.addObserver,
        instance.browserOpenNewtabStart, "browser-open-newtab-start");
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
    beforeEach(() => instance.init());
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
  describe("#sendEvent", () => {
    it("should call telemetrySender", async () => {
      sandbox.stub(instance.telemetrySender, "sendPing");
      const event = {};
      await instance.sendEvent(Promise.resolve(event));
      assert.calledWith(instance.telemetrySender.sendPing, event);
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
  });
  describe("#uninit", () => {
    it("should call .telemetrySender.uninit", () => {
      const stub = sandbox.stub(instance.telemetrySender, "uninit");
      instance.uninit();
      assert.calledOnce(stub);
    });
    it("should make this.browserOpenNewtabStart() stop observing browser-open-newtab-start", async () => {
      await instance.init();
      sandbox.spy(Services.obs, "removeObserver");
      sandbox.stub(instance.telemetrySender, "uninit");

      await instance.uninit();

      assert.calledOnce(Services.obs.removeObserver);
      assert.calledWithExactly(Services.obs.removeObserver,
        instance.browserOpenNewtabStart, "browser-open-newtab-start");
    });
  });
  describe("#onAction", () => {
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
    it("should call .setLoadTriggerInfo() on NEW_TAB_INIT action", () => {
      const stub = sandbox.stub(instance, "setLoadTriggerInfo");

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
    it("should send an event on an TELEMETRY_UNDESIRED_EVENT action", () => {
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const eventCreator = sandbox.stub(instance, "createUndesiredEvent");
      const action = {type: at.TELEMETRY_UNDESIRED_EVENT};
      instance.onAction(action);
      assert.calledWith(eventCreator, action);
      assert.calledWith(sendEvent, eventCreator.returnValue);
    });
    it("should send an event on an TELEMETRY_USER_EVENT action", () => {
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const eventCreator = sandbox.stub(instance, "createUserEvent");
      const action = {type: at.TELEMETRY_USER_EVENT};
      instance.onAction(action);
      assert.calledWith(eventCreator, action);
      assert.calledWith(sendEvent, eventCreator.returnValue);
    });
    it("should send an event on an TELEMETRY_PERFORMANCE_EVENT action", () => {
      const sendEvent = sandbox.stub(instance, "sendEvent");
      const eventCreator = sandbox.stub(instance, "createPerformanceEvent");
      const action = {type: at.TELEMETRY_PERFORMANCE_EVENT};
      instance.onAction(action);
      assert.calledWith(eventCreator, action);
      assert.calledWith(sendEvent, eventCreator.returnValue);
    });
  });
});
