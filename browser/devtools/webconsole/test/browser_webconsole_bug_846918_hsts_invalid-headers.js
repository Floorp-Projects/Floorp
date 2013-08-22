 /* Any copyright is dedicated to the Public Domain.
  * http://creativecommons.org/publicdomain/zero/1.0/ */
/* Tests that errors about invalid HSTS security headers are logged
 *  to the web console */
const TEST_URI = "https://example.com/browser/browser/devtools/webconsole/test/test-bug-846918-hsts-invalid-headers.html";
const HSTS_INVALID_HEADER_MSG = "The site specified an invalid Strict-Transport-Security header.";

function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad, true);
    openConsole(null, function testHSTSErrorLogged (hud) {
      waitForMessages({
        webconsole: hud,
        messages: [
          {
          name: "Invalid HSTS header error displayed successfully",
          text: HSTS_INVALID_HEADER_MSG,
          category: CATEGORY_SECURITY,
          severity: SEVERITY_WARNING
        },
        ],
      }).then(finishTest);
    });
  }, true);
}
