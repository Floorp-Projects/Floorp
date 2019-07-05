/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  await target.attach();

  info("Test applying onFront to a front that will be created");
  const promise = new Promise(resolve => {
    target.onFront("accessibility", resolve);
  });
  const getFrontFront = await target.getFront("accessibility");
  const onFrontFront = await promise;
  is(
    getFrontFront,
    onFrontFront,
    "got the front instantiated in the future and it's the same"
  );

  info("Test applying onFront to an existing front");
  await new Promise(resolve => {
    target.onFront("accessibility", front => {
      is(
        front,
        getFrontFront,
        "got the already instantiated front and it's the same"
      );
      resolve();
    });
  });
});
