/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let stateBackup = ss.getBrowserState();

let statePinned = {windows:[{tabs:[
  {entries:[{url:"http://example.com#1"}], pinned: true}
]}]};

let state = {windows:[{tabs:[
  {entries:[{url:"http://example.com#1"}]},
  {entries:[{url:"http://example.com#2"}]},
  {entries:[{url:"http://example.com#3"}]},
  {entries:[{url:"http://example.com#4"}]},
]}]};

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    ss.setBrowserState(stateBackup);
  });

  window.addEventListener("SSWindowStateReady", function onReady() {
    window.removeEventListener("SSWindowStateReady", onReady, false);

    let firstProgress = true;

    gProgressListener.setCallback(function (browser, needsRestore, isRestoring) {
      if (firstProgress) {
        firstProgress = false;
        is(isRestoring, 3, "restoring 3 tabs concurrently");
      } else {
        ok(isRestoring <= 3, "restoring max. 2 tabs concurrently");
      }

      if (0 == needsRestore) {
        gProgressListener.unsetCallback();
        waitForFocus(finish);
      }
    });

    ss.setBrowserState(JSON.stringify(state));
  }, false);

  ss.setBrowserState(JSON.stringify(statePinned));
}
