/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  await target.attach();

  info("Cached front when getFront has not been called");
  let getCachedFront = target.getCachedFront("performance");
  ok(!getCachedFront, "no front exists");

  info("Cached front when getFront has been called but has not finished");
  const asyncFront = target.getFront("performance");
  getCachedFront = target.getCachedFront("performance");
  ok(!getCachedFront, "no front exists");

  info("Cached front when getFront has been called and has finished");
  const front = await asyncFront;
  getCachedFront = target.getCachedFront("performance");
  is(getCachedFront, front, "front is the same as async front");
});
