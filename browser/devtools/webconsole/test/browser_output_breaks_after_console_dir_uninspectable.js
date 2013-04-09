/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure that the Web Console output does not break after we try to call
// console.dir() for objects that are not inspectable.

function test()
{
  waitForExplicitFinish();

  addTab("data:text/html;charset=utf8,test for bug 773466");

  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openConsole(null, performTest);
  }, true);
}

function performTest(hud)
{
  hud.jsterm.clearOutput(true);

  hud.jsterm.execute("console.log('fooBug773466a')");
  hud.jsterm.execute("myObj = Object.create(null)");
  hud.jsterm.execute("console.dir(myObj)");
  waitForSuccess({
    name: "eval results are shown",
    validatorFn: function()
    {
      return hud.outputNode.querySelector(".webconsole-msg-inspector");
    },
    successFn: function()
    {
      isnot(hud.outputNode.textContent.indexOf("fooBug773466a"), -1,
            "fooBug773466a shows");
      ok(hud.outputNode.querySelector(".webconsole-msg-inspector"),
         "the console.dir() tree shows");

      content.console.log("fooBug773466b");

      waitForSuccess(waitForAnotherConsoleLogCall);
    },
    failureFn: finishTest,
  });

  let waitForAnotherConsoleLogCall = {
    name: "eval result after console.dir()",
    validatorFn: function()
    {
      return hud.outputNode.textContent.indexOf("fooBug773466b") > -1;
    },
    successFn: finishTest,
    failureFn: finishTest,
  };
}
