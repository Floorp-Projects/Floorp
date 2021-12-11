var frameLoadsPending = 2;

var callMasterFrame = true;
var testDone = false;

var masterFrameOrigin = "";
var slaveFrameOrigin = "";

var failureRegExp = new RegExp("^FAILURE");
var todoRegExp = new RegExp("^TODO");

const framePath = "/tests/dom/tests/mochitest/storageevent/";

window.addEventListener("message", onMessageReceived);

function onMessageReceived(event) {
  switch (event.data) {
    // Indication of the frame onload event
    case "frame loaded":
      if (--frameLoadsPending) {
        break;
      }

    // Indication of successfully finished step of a test
    // Just fall through...
    case "perf":
      if (callMasterFrame) {
        masterFrame.postMessage("step", "*");
      } else if (slaveFrame) {
        slaveFrame.postMessage("step", "*");
      } else if (masterFrame.slaveFrame) {
        masterFrame.slaveFrame.postMessage("step", "*");
      }
      callMasterFrame = !callMasterFrame;
      break;

    // Indication of all test parts finish (from any of the frames)
    case "done":
      if (testDone) {
        break;
      }

      testDone = true;
      SimpleTest.finish();
      break;

    // Any other message indicates error, succes or todo message of a test
    default:
      if (typeof event.data == "undefined") {
        break;
      } // XXXkhuey this receives undefined values
      // (which used to become empty strings) on occasion ...
      if (event.data.match(todoRegExp)) {
        SimpleTest.todo(false, event.data);
      } else {
        SimpleTest.ok(!event.data.match(failureRegExp), event.data);
      }
      break;
  }
}
