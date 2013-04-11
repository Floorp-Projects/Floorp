/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that objects given to console.log() are inspectable.

function test()
{
  waitForExplicitFinish();

  addTab("data:text/html;charset=utf8,test for bug 676722 - inspectable objects for window.console");

  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openConsole(null, performTest);
  }, true);
}

function performTest(hud)
{
  hud.jsterm.clearOutput(true);

  hud.jsterm.execute("myObj = {abba: 'omgBug676722'}");
  hud.jsterm.execute("console.log('fooBug676722', myObj)");
  waitForSuccess({
    name: "eval results are shown",
    validatorFn: function()
    {
      return hud.outputNode.textContent.indexOf("fooBug676722") > -1 &&
             hud.outputNode.querySelector(".hud-clickable");
    },
    successFn: function()
    {
      isnot(hud.outputNode.textContent.indexOf("myObj = {"), -1,
            "myObj = ... is shown");

      let clickable = hud.outputNode.querySelector(".hud-clickable");
      ok(clickable, "the console.log() object .hud-clickable was found");
      isnot(clickable.textContent.indexOf("Object"), -1,
            "clickable node content is correct");

      hud.jsterm.once("variablesview-fetched",
        (aEvent, aVar) => {
          ok(aVar, "object inspector opened on click");

          findVariableViewProperties(aVar, [{
            name: "abba",
            value: "omgBug676722",
          }], { webconsole: hud }).then(finishTest);
        });

      executeSoon(function() {
        EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);
      });
    },
    failureFn: finishTest,
  });
}
