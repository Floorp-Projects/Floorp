/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  // set max rows to 1, to avoid scroll events by clicking middle button
  yield pushPrefs(["browser.newtabpage.rows", 1]);
  yield setLinks("0");
  yield* addNewTabPageTab();
  // we need a second newtab to honor max rows
  yield* addNewTabPageTab();

  yield ContentTask.spawn(gBrowser.selectedBrowser, {index: 0}, function* (args) {
    let {site} = content.wrappedJSObject.gGrid.cells[args.index];

    let origOnClick = site.onClick;
    site.onClick = e => {
      origOnClick.call(site, e);
      sendAsyncMessage("test:clicked-on-cell", {});
    };
  });

  let mm = gBrowser.selectedBrowser.messageManager;
  let messagePromise = new Promise(resolve => {
    mm.addMessageListener("test:clicked-on-cell", function onResponse(message) {
      mm.removeMessageListener("test:clicked-on-cell", onResponse);
      resolve();
    });
  });

  // Send a middle-click and make sure it happened
  yield BrowserTestUtils.synthesizeMouseAtCenter(".newtab-control-block",
                                                 {button: 1}, gBrowser.selectedBrowser);

  yield messagePromise;
  ok(true, "middle click triggered click listener");

  // Make sure the cell didn't actually get blocked
  yield* checkGrid("0");
});
