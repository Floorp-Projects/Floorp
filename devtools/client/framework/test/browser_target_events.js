/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  const target = TargetFactory.forTab(gBrowser.selectedTab);
  await target.makeRemote();
  is(target.tab, gBrowser.selectedTab, "Target linked to the right tab.");

  const willNavigate = once(target, "will-navigate");
  const navigate = once(target, "navigate");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.location = "data:text/html,<meta charset='utf8'/>test navigation";
  });
  await willNavigate;
  ok(true, "will-navigate event received");
  await navigate;
  ok(true, "navigate event received");

  const close = once(target, "close");
  gBrowser.removeCurrentTab();
  await close;
  ok(true, "close event received");
});
