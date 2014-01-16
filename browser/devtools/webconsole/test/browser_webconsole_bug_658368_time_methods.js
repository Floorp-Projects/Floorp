/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the Console API implements the time() and timeEnd() methods.

function test() {
  addTab("http://example.com/browser/browser/devtools/webconsole/" +
         "test/test-bug-658368-time-methods.html");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    Task.spawn(runner);
  }, true);

  function* runner() {
    let hud1 = yield openConsole();

    yield waitForMessages({
      webconsole: hud1,
      messages: [{
        name: "aTimer started",
        consoleTime: "aTimer",
      }, {
        name: "aTimer end",
        consoleTimeEnd: "aTimer",
      }],
    });

    let deferred = promise.defer();

    // The next test makes sure that timers with the same name but in separate
    // tabs, do not contain the same value.
    addTab("data:text/html;charset=utf-8,<script>" +
           "console.timeEnd('bTimer');</script>");
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      openConsole().then((hud) => {
        deferred.resolve(hud);
      });
    }, true);

    let hud2 = yield deferred.promise;

    testLogEntry(hud2.outputNode, "bTimer: timer started",
                 "bTimer was not started", false, true);

    // The next test makes sure that timers with the same name but in separate
    // pages, do not contain the same value.
    content.location = "data:text/html;charset=utf-8,<script>" +
                       "console.time('bTimer');</script>";

    yield waitForMessages({
      webconsole: hud2,
      messages: [{
        name: "bTimer started",
        consoleTime: "bTimer",
      }],
    });

    hud2.jsterm.clearOutput();

    deferred = promise.defer();

    // Now the following console.timeEnd() call shouldn't display anything,
    // if the timers in different pages are not related.
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      deferred.resolve(null);
    }, true);

    content.location = "data:text/html;charset=utf-8," +
                       "<script>console.timeEnd('bTimer');</script>";

    yield deferred.promise;

    testLogEntry(hud2.outputNode, "bTimer: timer started",
                 "bTimer was not started", false, true);

    yield closeConsole(gBrowser.selectedTab);

    gBrowser.removeCurrentTab();

    executeSoon(finishTest);
  }
}
