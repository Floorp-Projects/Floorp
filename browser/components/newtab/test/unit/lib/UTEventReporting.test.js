import { UTSessionPing, UTUserEventPing } from "test/schemas/pings";
import { GlobalOverrider } from "test/unit/utils";
import { UTEventReporting } from "lib/UTEventReporting.sys.mjs";

const FAKE_EVENT_PING_PC = {
  event: "CLICK",
  source: "TOP_SITES",
  addon_version: "123",
  user_prefs: 63,
  session_id: "abc",
  page: "about:newtab",
  action_position: 5,
  locale: "en-US",
};
const FAKE_SESSION_PING_PC = {
  session_duration: 1234,
  addon_version: "123",
  user_prefs: 63,
  session_id: "abc",
  page: "about:newtab",
  locale: "en-US",
};
const FAKE_EVENT_PING_UT = [
  "activity_stream",
  "event",
  "CLICK",
  "TOP_SITES",
  {
    addon_version: "123",
    user_prefs: "63",
    session_id: "abc",
    page: "about:newtab",
    action_position: "5",
  },
];
const FAKE_SESSION_PING_UT = [
  "activity_stream",
  "end",
  "session",
  "1234",
  {
    addon_version: "123",
    user_prefs: "63",
    session_id: "abc",
    page: "about:newtab",
  },
];

describe("UTEventReporting", () => {
  let globals;
  let sandbox;
  let utEvents;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    sandbox.stub(global.Services.telemetry, "setEventRecordingEnabled");
    sandbox.stub(global.Services.telemetry, "recordEvent");

    utEvents = new UTEventReporting();
  });

  afterEach(() => {
    globals.restore();
  });

  describe("#sendUserEvent()", () => {
    it("should queue up the correct data to send to Events Telemetry", async () => {
      utEvents.sendUserEvent(FAKE_EVENT_PING_PC);
      assert.calledWithExactly(
        global.Services.telemetry.recordEvent,
        ...FAKE_EVENT_PING_UT
      );

      let ping = global.Services.telemetry.recordEvent.firstCall.args;
      assert.validate(ping, UTUserEventPing);
    });
  });

  describe("#sendSessionEndEvent()", () => {
    it("should queue up the correct data to send to Events Telemetry", async () => {
      utEvents.sendSessionEndEvent(FAKE_SESSION_PING_PC);
      assert.calledWithExactly(
        global.Services.telemetry.recordEvent,
        ...FAKE_SESSION_PING_UT
      );

      let ping = global.Services.telemetry.recordEvent.firstCall.args;
      assert.validate(ping, UTSessionPing);
    });
  });

  describe("#uninit()", () => {
    it("should call setEventRecordingEnabled with a false value", () => {
      assert.equal(
        global.Services.telemetry.setEventRecordingEnabled.firstCall.args[0],
        "activity_stream"
      );
      assert.equal(
        global.Services.telemetry.setEventRecordingEnabled.firstCall.args[1],
        true
      );

      utEvents.uninit();
      assert.equal(
        global.Services.telemetry.setEventRecordingEnabled.secondCall.args[0],
        "activity_stream"
      );
      assert.equal(
        global.Services.telemetry.setEventRecordingEnabled.secondCall.args[1],
        false
      );
    });
  });
});
