/* Check presence of the "Ignore this warning" button */

function onDOMContentLoaded(callback) {
  function complete({ data }) {
    mm.removeMessageListener("Test:DOMContentLoaded", complete);
    callback(data);
  }

  let mm = gBrowser.selectedBrowser.messageManager;
  mm.addMessageListener("Test:DOMContentLoaded", complete);

  function contentScript() {
    let listener = function () {
      removeEventListener("DOMContentLoaded", listener);

      let button = content.document.getElementById("ignoreWarningButton");

      sendAsyncMessage("Test:DOMContentLoaded", { buttonPresent: !!button });
    };
    addEventListener("DOMContentLoaded", listener);
  }
  mm.loadFrameScript("data:,(" + contentScript.toString() + ")();", true);
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab("http://www.itisatrap.org/firefox/its-an-attack.html");
  onDOMContentLoaded(testMalware);
}

function testMalware(data) {
  ok(data.buttonPresent, "Ignore warning button should be present for malware");

  Services.prefs.setBoolPref("browser.safebrowsing.allowOverride", false);

  // Now launch the unwanted software test
  onDOMContentLoaded(testUnwanted);
  gBrowser.loadURI("http://www.itisatrap.org/firefox/unwanted.html");
}

function testUnwanted(data) {
  // Confirm that "Ignore this warning" is visible - bug 422410
  ok(!data.buttonPresent, "Ignore warning button should be missing for unwanted software");

  Services.prefs.setBoolPref("browser.safebrowsing.allowOverride", true);

  // Now launch the phishing test
  onDOMContentLoaded(testPhishing);
  gBrowser.loadURI("http://www.itisatrap.org/firefox/its-a-trap.html");
}

function testPhishing(data) {
  ok(data.buttonPresent, "Ignore warning button should be present for phishing");

  gBrowser.removeCurrentTab();
  finish();
}
