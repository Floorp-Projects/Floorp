/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = 'data:text/html;charset=utf-8,<div style="-moz-opacity:0;">test repeated' +
                 ' css warnings</div><p style="-moz-opacity:0">hi</p>';

function onContentLoaded()
{
  browser.removeEventListener("load", arguments.callee, true);

  let HUD = HUDService.getHudByWindow(content);
  let jsterm = HUD.jsterm;
  let outputNode = HUD.outputNode;

  let msg = "The unknown CSS property warning is displayed only once";
  let node = outputNode.firstChild;

  is(node.childNodes[2].textContent, "Unknown property '-moz-opacity'.  Declaration dropped.", "correct node")
  is(node.childNodes[3].firstChild.getAttribute("value"), 2, msg);

  jsterm.clearOutput();

  jsterm.setInputValue("for (let i = 0; i < 10; ++i) console.log('this is a line of reasonably long text that I will use to verify that the repeated text node is of an appropriate size.');");
  jsterm.execute();

  let msg = "The console output is repeated 10 times";
  let node = outputNode.querySelector(".webconsole-msg-console");
  is(node.childNodes[3].firstChild.getAttribute("value"), 10, msg);

  jsterm.clearOutput();
  finishTest();
}

/**
 * Unit test for bug 611795:
 * Repeated CSS messages get collapsed into one.
 */
function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    openConsole();
    // Clear cached messages that are shown once the Web Console opens.
    HUDService.getHudByWindow(content).jsterm.clearOutput(true);

    browser.addEventListener("load", onContentLoaded, true);
    content.location.reload();
  }, true);
}
