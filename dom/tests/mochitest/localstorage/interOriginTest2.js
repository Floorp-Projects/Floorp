var t = async_test(document.title);

var frameLoadsPending = 2;

var callMasterFrame = true;
var testDone = false;

var masterFrameOrigin = "";
var slaveFrameOrigin = "";

var failureRegExp = new RegExp("^FAILURE");

const framePath = "/tests/dom/tests/mochitest/localstorage/";

window.addEventListener("message", onMessageReceived, false);

function onMessageReceived(event)
{
  switch (event.data)
  {
    // Indication of the frame onload event
    case "frame loaded":
      if (--frameLoadsPending)
        break;

      // Just fall through...

    // Indication of successfully finished step of a test
    case "perf":
      if (callMasterFrame)
        masterFrame.postMessage("step", masterFrameOrigin);
      else
        slaveFrame.postMessage("step", slaveFrameOrigin);
      callMasterFrame = !callMasterFrame;
      break;

    // Indication of all test parts finish (from any of the frames)
    case "done":
      if (testDone)
        break;

      testDone = true;
      t.done();
      break;

    // Any other message indicates error, succes or todo message of a test
    default:
      t.step(function() {
        assert_true(!event.data.match(failureRegExp), event.data);
      });
      break;
  }
}
