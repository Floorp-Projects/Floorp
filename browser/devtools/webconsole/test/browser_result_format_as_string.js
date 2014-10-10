/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure that JS eval result are properly formatted as strings.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-result-format-as-string.html";

function test()
{
  waitForExplicitFinish();

  addTab(TEST_URI);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openConsole(null, performTest);
  }, true);
}

function performTest(hud)
{
  hud.jsterm.clearOutput(true);

  hud.jsterm.execute("document.querySelector('p')", (msg) => {
    is(hud.outputNode.textContent.indexOf("bug772506_content"), -1,
       "no content element found");
    ok(!hud.outputNode.querySelector("#foobar"), "no #foobar element found");

    ok(msg, "eval output node found");
    is(msg.textContent.indexOf("<div>"), -1,
       "<div> string is not displayed");
    isnot(msg.textContent.indexOf("<p>"), -1,
          "<p> string is displayed");

    EventUtils.synthesizeMouseAtCenter(msg, {type: "mousemove"});
    ok(!gBrowser._bug772506, "no content variable");

    finishTest();
  });
}
