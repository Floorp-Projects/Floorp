/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "data:text/html;charset=utf-8,<div style='font-size:3em;" +
  "foobarCssParser:baz'>test CSS parser filter</div>";

function onContentLoaded()
{
  browser.removeEventListener("load", onContentLoaded, true);

  let HUD = HUDService.getHudByWindow(content);
  let hudId = HUD.hudId;
  let outputNode = HUD.outputNode;

  HUD.jsterm.clearOutput();

  waitForSuccess({
    name: "css error displayed",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("foobarCssParser") > -1;
    },
    successFn: function()
    {
      HUD.setFilterState("cssparser", false);

      let msg = "the unknown CSS property warning is not displayed, " +
                "after filtering";
      testLogEntry(outputNode, "foobarCssParser", msg, true, true);

      HUD.setFilterState("cssparser", true);
      finishTest();
    },
    failureFn: finishTest,
  });
}

/**
 * Unit test for bug 589162:
 * CSS filtering on the console does not work
 */
function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    openConsole(null, function() {
      browser.addEventListener("load", onContentLoaded, true);
      content.location.reload();
    });
  }, true);
}

