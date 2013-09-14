/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that long strings can be expanded in the console output.

function test()
{
  waitForExplicitFinish();

  let tempScope = {};
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm", tempScope);
  let DebuggerServer = tempScope.DebuggerServer;

  let longString = (new Array(DebuggerServer.LONG_STRING_LENGTH + 4)).join("a") +
                   "foobar";
  let initialString =
    longString.substring(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH);

  addTab("data:text/html;charset=utf8,test for bug 787981 - check that long strings can be expanded in the output.");

  let hud = null;

  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openConsole(null, performTest);
  }, true);

  function performTest(aHud)
  {
    hud = aHud;
    hud.jsterm.clearOutput(true);
    hud.jsterm.execute("console.log('bazbaz', '" + longString +"', 'boom')");

    waitForMessages({
      webconsole: hud,
      messages: [{
        name: "console.log output",
        text: ["bazbaz", "boom", initialString],
        noText: "foobar",
        longString: true,
      }],
    }).then(onConsoleMessage);
  }

  function onConsoleMessage([result])
  {
    let clickable = result.longStrings[0];
    ok(clickable, "long string ellipsis is shown");

    clickable.scrollIntoView(false);

    EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);

    waitForMessages({
      webconsole: hud,
      messages: [{
        name: "full string",
        text: ["bazbaz", "boom", longString],
        category: CATEGORY_WEBDEV,
        longString: false,
      }],
    }).then(() => {
      hud.jsterm.clearOutput(true);
      hud.jsterm.execute("'" + longString +"'", onExecute);
    });
  }

  function onExecute(msg)
  {
    isnot(msg.textContent.indexOf(initialString), -1,
        "initial string is shown");
    is(msg.textContent.indexOf(longString), -1,
        "full string is not shown");

    let clickable = msg.querySelector(".longStringEllipsis");
    ok(clickable, "long string ellipsis is shown");

    clickable.scrollIntoView(false);

    EventUtils.synthesizeMouse(clickable, 3, 4, {}, hud.iframeWindow);

    waitForMessages({
      webconsole: hud,
      messages: [{
        name: "full string",
        text: longString,
        category: CATEGORY_OUTPUT,
        longString: false,
      }],
    }).then(finishTest);
  }
}
