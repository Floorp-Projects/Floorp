/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// A for-of loop in Web Console code can loop over a content NodeList.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-for-of.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testForOf, false);
}

function testForOf() {
  browser.removeEventListener("DOMContentLoaded", testForOf, false);

  openConsole();
  var hud = HUDService.getHudByWindow(content);
  var jsterm = hud.jsterm;
  jsterm.execute("{ [x.tagName for (x of document.body.childNodes) if (x.nodeType === 1)].join(' '); }");

  let node = hud.outputNode.querySelector(".webconsole-msg-output");
  ok(/H1 DIV H2 P/.test(node.textContent),
    "for-of loop should find all top-level nodes");

  jsterm.clearOutput();
  jsterm.history.splice(0, jsterm.history.length);   // workaround for bug 592552
  finishTest();
}
