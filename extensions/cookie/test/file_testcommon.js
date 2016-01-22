const SCRIPT_URL = SimpleTest.getTestFileURL("file_chromecommon.js");

var gExpectedCookies;
var gExpectedLoads;

var gPopup;

var gScript;

var gLoads = 0;

function setupTest(uri, cookies, loads) {
  SimpleTest.waitForExplicitFinish();

  var prefSet = new Promise(resolve => {
    SpecialPowers.pushPrefEnv({ set: [["network.cookie.cookieBehavior", 1]] }, resolve);
  });

  gScript = SpecialPowers.loadChromeScript(SCRIPT_URL);
  gExpectedCookies = cookies;
  gExpectedLoads = loads;

  // Listen for MessageEvents.
  window.addEventListener("message", messageReceiver, false);

  prefSet.then(() => {
    // load a window which contains an iframe; each will attempt to set
    // cookies from their respective domains.
    gPopup = window.open(uri, 'hai', 'width=100,height=100');
  });
}

function finishTest() {
  gScript.destroy();
  SimpleTest.finish();
}

/** Receives MessageEvents to this window. */
// Count and check loads.
function messageReceiver(evt) {
  is(evt.data, "message", "message data received from popup");
  if (evt.data != "message") {
    gPopup.close();
    window.removeEventListener("message", messageReceiver, false);

    finishTest();
    return;
  }

  // only run the test when all our children are done loading & setting cookies
  if (++gLoads == gExpectedLoads) {
    gPopup.close();
    window.removeEventListener("message", messageReceiver, false);

    runTest();
  }
}

// runTest() is run by messageReceiver().
// Count and check cookies.
function runTest() {
  // set a cookie from a domain of "localhost"
  document.cookie = "oh=hai";

  gScript.addMessageListener("getCookieCountAndClear:return", ({ count }) => {
    is(count, gExpectedCookies, "total number of cookies");
    finishTest();
  });
  gScript.sendAsyncMessage("getCookieCountAndClear");
}
