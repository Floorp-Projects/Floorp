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

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fooBug773466a",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    },
    {
      name: "console.dir output",
      consoleDir: "[object Object]",
    }],
  }).then(() => {
    content.console.log("fooBug773466b");
    waitForMessages({
      webconsole: hud,
      messages: [{
        text: "fooBug773466b",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    }).then(finishTest);
  });
}
