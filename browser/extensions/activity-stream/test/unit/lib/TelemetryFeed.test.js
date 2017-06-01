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
  let store = {getState() { return {App: {version: "1.0.0", locale: "en-US"}}; }};
  let instance;
  class TelemetrySender {sendPing() {} uninit() {}}
  const {TelemetryFeed} = injector({"lib/TelemetrySender.jsm": {TelemetrySender}});

  function addSession(id) {
    instance.addSession(id);
    return instance.sessions.get(id);
  }

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
    it("should add .telemetrySender, a TelemetrySender instance", async () => {
      assert.isNull(instance.telemetrySender);
      await instance.init();
      assert.instanceOf(instance.telemetrySender, TelemetrySender);
    });
    it("should add .telemetryClientId from the ClientID module", async () => {
      assert.isNull(instance.telemetryClientId);
      await instance.init();
      assert.equal(instance.telemetryClientId, FAKE_TELEMETRY_ID);
    });
  });
  describe("#addSession", () => {
    it("should add a session", () => {
      addSession("foo");
      assert.isTrue(instance.sessions.has("foo"));
    });
    it("should set the start_time", () => {
      sandbox.spy(Components.utils, "now");
      const session = addSession("foo");
      assert.calledOnce(Components.utils.now);
      assert.equal(session.start_time, Components.utils.now.firstCall.returnValue);
    });
    it("should set the session_id", () => {
      sandbox.spy(global.gUUIDGenerator, "generateUUID");
      const session = addSession("foo");
      assert.calledOnce(global.gUUIDGenerator.generateUUID);
      assert.equal(session.session_id, global.gUUIDGenerator.generateUUID.firstCall.returnValue);
    });
    it("should set the page", () => {
      const session = addSession("foo");
      assert.equal(session.page, "about:newtab"); // This is hardcoded for now.
    });
  });
  describe("#endSession", () => {
    it("should not throw if there is no session for the given port ID", () => {
      assert.doesNotThrow(() => instance.endSession("doesn't exist"));
    });
    it("should add a session_duration", () => {
      sandbox.stub(instance, "sendEvent");
      const session = addSession("foo");
      instance.endSession("foo");
      assert.property(session, "session_duration");
    });
    it("should remove the session from .sessions", () => {
      sandbox.stub(instance, "sendEvent");
      addSession("foo");
      instance.endSession("foo");
      assert.isFalse(instance.sessions.has("foo"));
    });
    it("should call createSessionSendEvent and sendEvent with the sesssion", () => {
      sandbox.stub(instance, "sendEvent");
      sandbox.stub(instance, "createSessionEndEvent");
      const session = addSession("foo");
      instance.endSession("foo");

      // Did we call sendEvent with the result of createSessionEndEvent?
      assert.calledWith(instance.createSessionEndEvent, session);
      assert.calledWith(instance.sendEvent, instance.createSessionEndEvent.firstCall.returnValue);
    });
  });
  describe("ping creators", () => {
    beforeEach(async () => await instance.init());
    describe("#createPing", () => {
      it("should create a valid base ping without a session if no portID is supplied", () => {
        const ping = instance.createPing();
        assert.validate(ping, BasePing);
        assert.notProperty(ping, "session_id");
      });
      it("should create a valid base ping with session info if a portID is supplied", () => {
        // Add a session
        const portID = "foo";
        instance.addSession(portID);
        const sessionID = instance.sessions.get(portID).session_id;

        // Create a ping referencing the session
        const ping = instance.createPing(portID);
        assert.validate(ping, BasePing);

        // Make sure we added the right session-related stuff to the ping
        assert.propertyVal(ping, "session_id", sessionID);
        assert.propertyVal(ping, "page", "about:newtab");
      });
    });
    describe("#createUserEvent", () => {
      it("should create a valid event", () => {
        const portID = "foo";
        const data = {source: "TOP_SITES", event: "CLICK"};
        const action = ac.SendToMain(ac.UserEvent(data), portID);
        const session = addSession(portID);
        const ping = instance.createUserEvent(action);

        // Is it valid?
        assert.validate(ping, UserEventPing);
        // Does it have the right session_id?
        assert.propertyVal(ping, "session_id", session.session_id);
      });
    });
    describe("#createUndesiredEvent", () => {
      it("should create a valid event without a session", () => {
        const action = ac.UndesiredEvent({source: "TOP_SITES", event: "MISSING_IMAGE", value: 10});
        const ping = instance.createUndesiredEvent(action);

        // Is it valid?
        assert.validate(ping, UndesiredPing);
        // Does it have the right value?
        assert.propertyVal(ping, "value", 10);
      });
      it("should create a valid event with a session", () => {
        const portID = "foo";
        const data = {source: "TOP_SITES", event: "MISSING_IMAGE", value: 10};
        const action = ac.SendToMain(ac.UndesiredEvent(data), portID);
        const session = addSession(portID);
        const ping = instance.createUndesiredEvent(action);

        // Is it valid?
        assert.validate(ping, UndesiredPing);
        // Does it have the right session_id?
        assert.propertyVal(ping, "session_id", session.session_id);
        // Does it have the right value?
        assert.propertyVal(ping, "value", 10);
      });
    });
    describe("#createPerformanceEvent", () => {
      it("should create a valid event without a session", () => {
        const action = ac.PerfEvent({event: "SCREENSHOT_FINISHED", value: 100});
        const ping = instance.createPerformanceEvent(action);

        // Is it valid?
        assert.validate(ping, PerfPing);
        // Does it have the right value?
        assert.propertyVal(ping, "value", 100);
      });
      it("should create a valid event with a session", () => {
        const portID = "foo";
        const data = {event: "PAGE_LOADED", value: 100};
        const action = ac.SendToMain(ac.PerfEvent(data), portID);
        const session = addSession(portID);
        const ping = instance.createPerformanceEvent(action);

        // Is it valid?
        assert.validate(ping, PerfPing);
        // Does it have the right session_id?
        assert.propertyVal(ping, "session_id", session.session_id);
        // Does it have the right value?
        assert.propertyVal(ping, "value", 100);
      });
    });
    describe("#createSessionEndEvent", () => {
      it("should create a valid event", () => {
        const ping = instance.createSessionEndEvent({
          session_id: FAKE_UUID,
          page: "about:newtab",
          session_duration: 12345
        });
        // Is it valid?
        assert.validate(ping, SessionPing);
        assert.propertyVal(ping, "session_id", FAKE_UUID);
        assert.propertyVal(ping, "page", "about:newtab");
        assert.propertyVal(ping, "session_duration", 12345);
      });
    });
  });
  describe("#sendEvent", () => {
    it("should call telemetrySender", async () => {
      await instance.init();
      sandbox.stub(instance.telemetrySender, "sendPing");
      const event = {};
      instance.sendEvent(event);
      assert.calledWith(instance.telemetrySender.sendPing, event);
    });
  });
  describe("#uninit", () => {
    it("should call .telemetrySender.uninit and remove it", async () => {
      await instance.init();
      const stub = sandbox.stub(instance.telemetrySender, "uninit");
      instance.uninit();
      assert.calledOnce(stub);
      assert.isNull(instance.telemetrySender);
    });
  });
  describe("#onAction", () => {
    it("should call .init() on an INIT action", () => {
      const stub = sandbox.stub(instance, "init");
      instance.onAction({type: at.INIT});
      assert.calledOnce(stub);
    });
    it("should call .addSession() on a NEW_TAB_VISIBLE action", () => {
      const stub = sandbox.stub(instance, "addSession");
      instance.onAction(ac.SendToMain({type: at.NEW_TAB_VISIBLE}, "port123"));
      assert.calledWith(stub, "port123");
    });
    it("should call .endSession() on a NEW_TAB_UNLOAD action", () => {
      const stub = sandbox.stub(instance, "endSession");
      instance.onAction(ac.SendToMain({type: at.NEW_TAB_UNLOAD}, "port123"));
      assert.calledWith(stub, "port123");
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
