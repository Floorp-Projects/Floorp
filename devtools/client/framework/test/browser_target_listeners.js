/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  const target = await createAndAttachTargetForTab(gBrowser.selectedTab);

  info("Test applying watchFronts to a front that will be created");
  const promise = new Promise(resolve => {
    target.watchFronts("accessibility", resolve);
  });
  const getFrontFront = await target.getFront("accessibility");
  const watchFrontsFront = await promise;
  is(
    getFrontFront,
    watchFrontsFront,
    "got the front instantiated in the future and it's the same"
  );

  info("Test applying watchFronts to an existing front");
  await new Promise(resolve => {
    target.watchFronts("accessibility", front => {
      is(
        front,
        getFrontFront,
        "got the already instantiated front and it's the same"
      );
      resolve();
    });
  });
});
