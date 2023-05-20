/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  const target = await createAndAttachTargetForTab(gBrowser.selectedTab);

  info("Cached front when getFront has not been called");
  let getCachedFront = target.getCachedFront("accessibility");
  ok(!getCachedFront, "no front exists");

  info("Cached front when getFront has been called but has not finished");
  const asyncFront = target.getFront("accessibility");
  getCachedFront = target.getCachedFront("accessibility");
  ok(!getCachedFront, "no front exists");

  info("Cached front when getFront has been called and has finished");
  const front = await asyncFront;
  getCachedFront = target.getCachedFront("accessibility");
  is(getCachedFront, front, "front is the same as async front");
});
