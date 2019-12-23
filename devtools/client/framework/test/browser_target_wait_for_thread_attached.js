/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the target-mixin onThreadAttached promise resolves when the
 * target's thread front is fully attached.
 */
add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  await target.attach();
  ok(
    !target.threadFront,
    "target has no threadFront property before calling attachThread"
  );

  info("Call attachThread and wait for onThreadAttached to resolve");
  const onThreadAttached = target.onThreadAttached;
  target.attachThread();

  info("Wait for the onThreadAttached promise to resolve on the target");
  await onThreadAttached;

  ok(!!target.threadFront, "target has a threadFront property");
  is(target.threadFront.state, "paused", "threadFront is in paused state");

  const close = once(target, "close");
  gBrowser.removeCurrentTab();
  await close;
});
