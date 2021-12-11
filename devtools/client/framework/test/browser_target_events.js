/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  // Navigation events (navigate/will-navigate) on the target no longer fire with server targets.
  // And as bfcache in parent introduce server target, they are also missing in this case.
  // We should probably drop this test once we stop supporting client side targets (bug 1721852).
  await pushPref("devtools.target-switching.server.enabled", false);

  // Disable bfcache for Fission for now.
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  const target = await createAndAttachTargetForTab(gBrowser.selectedTab);
  is(target.localTab, gBrowser.selectedTab, "Target linked to the right tab.");

  const willNavigate = once(target, "will-navigate");
  const navigate = once(target, "navigate");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.location = "data:text/html,<meta charset='utf8'/>test navigation";
  });
  await willNavigate;
  ok(true, "will-navigate event received");
  await navigate;
  ok(true, "navigate event received");

  const onTargetDestroyed = once(target, "target-destroyed");
  gBrowser.removeCurrentTab();
  await onTargetDestroyed;
  ok(true, "target destroyed received");
});
