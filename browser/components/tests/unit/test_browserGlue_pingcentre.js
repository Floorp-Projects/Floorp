ChromeUtils.import("resource:///modules/PingCentre.jsm");

const TOPIC_SHIELD_INIT_COMPLETE = "shield-init-complete";
const SEND_PING_MOCK = sinon.stub(PingCentre.prototype, "sendPing");

let gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"]
                     .getService(Ci.nsIObserver);

add_task(async function() {
  // Simulate ping centre sendPing() trigger.
  gBrowserGlue.observe(null, TOPIC_SHIELD_INIT_COMPLETE, null);

  const SEND_PING_CALL_ARGS = {
    event: "AS_ENABLED",
    value: 0
  };
  const SEND_PING_FILTER = { filter: "activity-stream" };

  Assert.ok(SEND_PING_MOCK.called, "gBrowserGlue.pingCentre.sendPing() is called");
  Assert.ok(SEND_PING_MOCK.calledWithExactly(SEND_PING_CALL_ARGS, SEND_PING_FILTER),
    "sendPing() is called with the correct param");
});
