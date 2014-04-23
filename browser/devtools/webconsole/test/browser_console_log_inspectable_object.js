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

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fooBug676722",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      objects: true,
    }],
  }).then(([result]) => {
    let msg = [...result.matched][0];
    ok(msg, "message element");
    let body = msg.querySelector(".message-body");
    ok(body, "message body");
    let clickable = result.clickableElements[0];
    ok(clickable, "the console.log() object anchor was found");
    ok(body.textContent.contains('{ abba: "omgBug676722" }'),
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
  });
}
