/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that Dead Objects do not break the Web/Browser Consoles. See bug 883649.
// This test does:
// - opens a new tab,
// - opens the Browser Console,
// - stores a reference to the content document of the tab on the chrome window object,
// - closes the tab,
// - tries to use the object that was pointing to the now-defunct content
// document. This is the dead object.

const TEST_URI = "data:text/html;charset=utf8,<p>dead objects!";

function test()
{
  let hud = null;

  Task.spawn(runner).then(finishTest);

  function* runner() {
    let {tab} = yield loadTab(TEST_URI);

    info("open the browser console");

    hud = yield HUDService.toggleBrowserConsole();
    ok(hud, "browser console opened");

    hud.jsterm.clearOutput();

    // Add the reference to the content document.

    yield execute("Cu = Components.utils;" +
                  "Cu.import('resource://gre/modules/Services.jsm');" +
                  "chromeWindow = Services.wm.getMostRecentWindow('navigator:browser');" +
                  "foobarzTezt = chromeWindow.content.document;" +
                  "delete chromeWindow");

    gBrowser.removeCurrentTab();

    let msg = yield execute("foobarzTezt");

    isnot(hud.outputNode.textContent.indexOf("[object DeadObject]"), -1,
          "dead object found");

    hud.jsterm.setInputValue("foobarzTezt");

    for (let c of ".hello") {
      EventUtils.synthesizeKey(c, {}, hud.iframeWindow);
    }

    yield execute();

    isnot(hud.outputNode.textContent.indexOf("can't access dead object"), -1,
          "'cannot access dead object' message found");

    // Click the second execute output.
    let clickable = msg.querySelector("a");
    ok(clickable, "clickable object found");
    isnot(clickable.textContent.indexOf("[object DeadObject]"), -1,
          "message text check");

    msg.scrollIntoView();

    executeSoon(() => {
      EventUtils.synthesizeMouseAtCenter(clickable, {}, hud.iframeWindow);
    });

    yield hud.jsterm.once("variablesview-fetched");
    ok(true, "variables view fetched");

    msg = yield execute("delete window.foobarzTezt; 2013-26");

    isnot(msg.textContent.indexOf("1987"), -1, "result message found");
  }

  function execute(str) {
    let deferred = promise.defer();
    hud.jsterm.execute(str, (msg) => {
      deferred.resolve(msg);
    });
    return deferred.promise;
  }
}
