/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-603750-websocket.html";

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 603750: Web Socket errors");
  browser.addEventListener("load", function tabLoad() {
    browser.removeEventListener("load", tabLoad, true);
    openConsole(null, (hud) => {
      content.location = TEST_URI;
      info("waiting for websocket errors");
      waitForMessages({
        webconsole: hud,
        messages: [
          {
            text: "ws://0.0.0.0:81",
            source: { url: "test-bug-603750-websocket.js" },
            category: CATEGORY_JS,
            severity: SEVERITY_ERROR,
          },
          {
            text: "ws://0.0.0.0:82",
            source: { url: "test-bug-603750-websocket.js" },
            category: CATEGORY_JS,
            severity: SEVERITY_ERROR,
          },
        ]}).then(finishTest);
    });
  }, true);
}

