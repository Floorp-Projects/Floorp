/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield* addNewTabPageTab();
  yield* checkGrid("0,1,2,3,4,5,6,7,8");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.addEventListener("error", function() {
      sendAsyncMessage("test:newtab-error", {});
    });
  });

  let receivedError = false;
  let mm = gBrowser.selectedBrowser.messageManager;
  mm.addMessageListener("test:newtab-error", function onResponse(message) {
    mm.removeMessageListener("test:newtab-error", onResponse);
    ok(false, "Error event happened");
    receivedError = true;
  });

  let pagesUpdatedPromise = whenPagesUpdated();

  for (let i = 0; i < 3; i++) {
    yield BrowserTestUtils.synthesizeMouseAtCenter(".newtab-control-block", {}, gBrowser.selectedBrowser);
  }

  yield pagesUpdatedPromise;

  ok(!receivedError, "we got here without any errors");
});
