/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab("data:text/html,<iframe width='700' height='700'></iframe>");
  // Open a html page with about:certerror in an iframe
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    return ContentTask.spawn(gBrowser.selectedBrowser, "", function() {
      return new Promise(resolve => {
        info("Running content task");
        let listener = e => {
          removeEventListener('AboutNetErrorLoad', listener, false, true);
          resolve();
        };
        addEventListener('AboutNetErrorLoad', listener, false, true);
        let iframe = content.document.querySelector("iframe");
        iframe.src = "https://expired.example.com/";
      });
    }).then(testIframeCert);
  });
}

function testIframeCert(e) {
  // Confirm that the expert section is hidden
  var doc = gBrowser.contentDocument.getElementsByTagName('iframe')[0].contentDocument;
  var aP = doc.getElementById("badCertAdvancedPanel");
  ok(aP, "Advanced content should exist");
  is_element_hidden(aP, "Advanced content should not be visible by default")

  // Clean up
  gBrowser.removeCurrentTab();

  finish();
}
