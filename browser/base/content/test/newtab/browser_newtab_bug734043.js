/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  await addNewTabPageTab();
  await checkGrid("0,1,2,3,4,5,6,7,8");

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
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
    await BrowserTestUtils.synthesizeMouseAtCenter(".newtab-control-block", {}, gBrowser.selectedBrowser);
  }

  await pagesUpdatedPromise;

  ok(!receivedError, "we got here without any errors");
});
