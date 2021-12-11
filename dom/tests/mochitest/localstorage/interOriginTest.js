var slaveLoadsPending = 1;

var slaveOrigin = "";
var slave = null;

var failureRegExp = new RegExp("^FAILURE");
const slavePath = "/tests/dom/tests/mochitest/localstorage/";

window.addEventListener("message", onMessageReceived);

function onMessageReceived(event) {
  switch (event.data) {
    // Indication of the frame onload event
    case "frame loaded":
      if (--slaveLoadsPending) {
        break;
      }

    // Indication of successfully finished step of a test
    // fall through
    case "perf":
      if (event.data == "perf") {
        doStep();
      }

      slave.postMessage("step", slaveOrigin);
      break;

    // Indication of all test parts finish (from any of the frames)
    case "done":
      localStorage.clear();
      slaveLoadsPending = 1;
      doNextTest();
      break;

    // Any other message indicates error or succes message of a test
    default:
      SimpleTest.ok(!event.data.match(failureRegExp), event.data);
      break;
  }
}
