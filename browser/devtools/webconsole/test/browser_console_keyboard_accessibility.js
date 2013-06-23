/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that basic keyboard shortcuts work in the web console.

function test()
{
  const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";
  let hud = null;

  addTab(TEST_URI);

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);

  function consoleOpened(aHud)
  {
    hud = aHud;
    ok(hud, "Web Console opened");

    content.console.log("foobarz1");
    waitForMessages({
      webconsole: hud,
      messages: [{
        text: "foobarz1",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    }).then(onConsoleMessage);
  }

  function onConsoleMessage()
  {
    hud.jsterm.once("messages-cleared", onClear);
    info("try ctrl-k to clear output");
    EventUtils.synthesizeKey("K", { accelKey: true });
  }

  function onClear()
  {
    is(hud.outputNode.textContent.indexOf("foobarz1"), -1, "output cleared");
    is(hud.jsterm.inputNode.getAttribute("focused"), "true",
       "jsterm input is focused");

    info("try ctrl-f to focus filter");
    EventUtils.synthesizeKey("F", { accelKey: true });
    ok(!hud.jsterm.inputNode.getAttribute("focused"),
       "jsterm input is not focused");
    is(hud.ui.filterBox.getAttribute("focused"), "true",
       "filter input is focused");

    if (Services.appinfo.OS == "Darwin") {
      EventUtils.synthesizeKey("t", { ctrlKey: true });
    }
    else {
      EventUtils.synthesizeKey("N", { altKey: true });
    }

    let net = hud.ui.document.querySelector("toolbarbutton[category=net]");
    is(hud.ui.document.activeElement, net,
       "accesskey for Network category focuses the Net button");

    finishTest();
  }
}
