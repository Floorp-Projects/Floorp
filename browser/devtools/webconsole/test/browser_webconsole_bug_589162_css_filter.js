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
  browser.removeEventListener("load", arguments.callee, true);

  let HUD = HUDService.getHudByWindow(content);
  let hudId = HUD.hudId;
  let outputNode = HUD.outputNode;

  let msg = "the unknown CSS property warning is displayed";
  testLogEntry(outputNode, "foobarCssParser", msg, true);

  HUDService.setFilterState(hudId, "cssparser", false);

  executeSoon(
    function (){
      let msg = "the unknown CSS property warning is not displayed, " +
                "after filtering";
      testLogEntry(outputNode, "foobarCssParser", msg, true, true);

      HUDService.setFilterState(hudId, "cssparser", true);
      finishTest();
    }
  );
}

/**
 * Unit test for bug 589162:
 * CSS filtering on the console does not work
 */
function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    openConsole();
    browser.addEventListener("load", onContentLoaded, true);
    content.location.reload();
  }, true);
}

