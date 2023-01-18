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

  const commands = await CommandsFactory.forTab(gBrowser.selectedTab);
  is(
    commands.descriptorFront.localTab,
    gBrowser.selectedTab,
    "Target linked to the right tab."
  );

  await commands.targetCommand.startListening();
  const { targetFront } = commands.targetCommand;

  const willNavigate = once(targetFront, "will-navigate");
  const navigate = once(targetFront, "navigate");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.location = "data:text/html,<meta charset='utf8'/>test navigation";
  });
  await willNavigate;
  ok(true, "will-navigate event received");
  await navigate;
  ok(true, "navigate event received");

  const onTargetDestroyed = once(targetFront, "target-destroyed");
  gBrowser.removeCurrentTab();
  await onTargetDestroyed;
  ok(true, "target destroyed received");

  await commands.destroy();
});
