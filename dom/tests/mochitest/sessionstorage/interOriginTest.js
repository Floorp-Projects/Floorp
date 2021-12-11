var slaveLoadsPending = 1;

var slaveOrigin = "";
var slave = null;

var failureRegExp = new RegExp("^FAILURE");
const slavePath = "/tests/dom/tests/mochitest/sessionstorage/";

window.addEventListener("message", onMessageReceived);

function onMessageReceived(event) {
  //alert("master got event: "+event.data);
  switch (event.data) {
    // Indication of the frame onload event
    case "frame loaded":
      if (--slaveLoadsPending) {
        break;
      }

    // Indication of successfully finished step of a test
    // Just fall through...
    case "perf":
      // We called doStep before the frame was load
      if (event.data == "perf") {
        doStep();
      }

      slave.postMessage("step", slaveOrigin);
      break;

    // Indication of all test parts finish (from any of the frames)
    case "done":
      sessionStorage.clear();
      slaveLoadsPending = 1;
      doNextTest();
      break;

    // Any other message indicates error or succes message of a test
    default:
      SimpleTest.ok(!event.data.match(failureRegExp), event.data);
      break;
  }
}
