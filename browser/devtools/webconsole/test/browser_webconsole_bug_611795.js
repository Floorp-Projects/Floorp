/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = 'data:text/html;charset=utf-8,<div style="-moz-opacity:0;">test repeated' +
                 ' css warnings</div><p style="-moz-opacity:0">hi</p>';

function onContentLoaded()
{
  browser.removeEventListener("load", onContentLoaded, true);

  let HUD = HUDService.getHudByWindow(content);
  let jsterm = HUD.jsterm;
  let outputNode = HUD.outputNode;

  let cssWarning = "Unknown property '-moz-opacity'.  Declaration dropped.";

  waitForSuccess({
    name: "2 repeated CSS warnings",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf(cssWarning) > -1;
    },
    successFn: function()
    {
      let msg = "The unknown CSS property warning is displayed only once";
      let node = outputNode.firstChild;

      is(node.childNodes[2].textContent, cssWarning, "correct node");
      is(node.childNodes[3].firstChild.getAttribute("value"), 2, msg);

      testConsoleLogRepeats();
    },
    failureFn: finishTest,
  });
}

function testConsoleLogRepeats()
{
  let HUD = HUDService.getHudByWindow(content);
  let jsterm = HUD.jsterm;
  let outputNode = HUD.outputNode;

  jsterm.clearOutput();

  jsterm.setInputValue("for (let i = 0; i < 10; ++i) console.log('this is a line of reasonably long text that I will use to verify that the repeated text node is of an appropriate size.');");
  jsterm.execute();

  waitForSuccess({
    name: "10 repeated console.log messages",
    validatorFn: function()
    {
      let node = outputNode.querySelector(".webconsole-msg-console");
      return node && node.childNodes[3].firstChild.getAttribute("value") == 10;
    },
    successFn: finishTest,
    failureFn: finishTest,
  });
}

/**
 * Unit test for bug 611795:
 * Repeated CSS messages get collapsed into one.
 */
function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, function(aHud) {
      // Clear cached messages that are shown once the Web Console opens.
      aHud.jsterm.clearOutput(true);
      browser.addEventListener("load", onContentLoaded, true);
      content.location.reload();
    });
  }, true);
}
