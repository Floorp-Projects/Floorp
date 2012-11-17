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

    waitForSuccess(waitForConsoleLog);
  }

  let waitForConsoleLog = {
    name: "console.log output shown",
    validatorFn: function()
    {
      return hud.outputNode.querySelector(".webconsole-msg-console");
    },
    successFn: function()
    {
      let msg = hud.outputNode.querySelector(".webconsole-msg-console");
      is(msg.textContent.indexOf("foobar"), -1,
         "foobar is not shown");
      isnot(msg.textContent.indexOf("bazbaz"), -1,
            "bazbaz is shown");
      isnot(msg.textContent.indexOf("boom"), -1,
            "boom is shown");
      isnot(msg.textContent.indexOf(initialString), -1,
            "initial string is shown");

      let clickable = msg.querySelector(".longStringEllipsis");
      ok(clickable, "long string ellipsis is shown");

      scrollToVisible(clickable);

      executeSoon(function() {
        EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);
        waitForSuccess(waitForFullString);
      });
    },
    failureFn: finishTest,
  };

  let waitForFullString = {
    name: "full string shown",
    validatorFn: function()
    {
      let msg = hud.outputNode.querySelector(".webconsole-msg-log");
      return msg.textContent.indexOf(longString) > -1;
    },
    successFn: function()
    {
      let msg = hud.outputNode.querySelector(".webconsole-msg-log");
      isnot(msg.textContent.indexOf("bazbaz"), -1,
            "bazbaz is shown");
      isnot(msg.textContent.indexOf("boom"), -1,
            "boom is shown");

      let clickable = msg.querySelector(".longStringEllipsis");
      ok(!clickable, "long string ellipsis is not shown");

      executeSoon(function() {
        hud.jsterm.clearOutput(true);
        hud.jsterm.execute("'" + longString +"'");
        waitForSuccess(waitForExecute);
      });
    },
    failureFn: finishTest,
  };

  let waitForExecute = {
    name: "execute() output shown",
    validatorFn: function()
    {
      return hud.outputNode.querySelector(".webconsole-msg-output");
    },
    successFn: function()
    {
      let msg = hud.outputNode.querySelector(".webconsole-msg-output");
      isnot(msg.textContent.indexOf(initialString), -1,
           "initial string is shown");
      is(msg.textContent.indexOf(longString), -1,
         "full string is not shown");

      let clickable = msg.querySelector(".longStringEllipsis");
      ok(clickable, "long string ellipsis is shown");

      scrollToVisible(clickable);

      executeSoon(function() {
        EventUtils.synthesizeMouse(clickable, 3, 4, {}, hud.iframeWindow);
        waitForSuccess(waitForFullStringAfterExecute);
      });
    },
    failureFn: finishTest,
  };

  let waitForFullStringAfterExecute = {
    name: "full string shown again",
    validatorFn: function()
    {
      let msg = hud.outputNode.querySelector(".webconsole-msg-output");
      return msg.textContent.indexOf(longString) > -1;
    },
    successFn: function()
    {
      let msg = hud.outputNode.querySelector(".webconsole-msg-output");
      let clickable = msg.querySelector(".longStringEllipsis");
      ok(!clickable, "long string ellipsis is not shown");

      executeSoon(finishTest);
    },
    failureFn: finishTest,
  };

  function scrollToVisible(aNode)
  {
    let richListBoxNode = aNode.parentNode;
    while (richListBoxNode.tagName != "richlistbox") {
      richListBoxNode = richListBoxNode.parentNode;
    }

    let boxObject = richListBoxNode.scrollBoxObject;
    let nsIScrollBoxObject = boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    nsIScrollBoxObject.ensureElementIsVisible(aNode);
  }
}
