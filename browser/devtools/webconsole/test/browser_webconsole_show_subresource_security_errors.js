/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Ensure non-toplevel security errors are displayed

const TEST_URI = "data:text/html;charset=utf8,Web Console subresource STS warning test";
const TEST_DOC = "https://example.com/browser/browser/devtools/webconsole/test/test_bug1092055_shouldwarn.html";
const SAMPLE_MSG = 'invalid Strict-Transport-Security header'

let test = asyncTest(function* () {
  let { browser } = yield loadTab(TEST_URI);

  let hud = yield openConsole();

  hud.jsterm.clearOutput();

  let loaded = loadBrowser(browser);
  content.location = TEST_DOC;
  yield loaded;

  yield waitForSuccess({
    name: "Subresource STS warning displayed successfully",
    validator: function() {
      return hud.outputNode.textContent.indexOf(SAMPLE_MSG) > -1;
    }
  });
});
