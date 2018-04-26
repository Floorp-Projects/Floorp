/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  await target.makeRemote();
  is(target.tab, gBrowser.selectedTab, "Target linked to the right tab.");

  let hidden = once(target, "hidden");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await hidden;
  ok(true, "Hidden event received");

  let visible = once(target, "visible");
  gBrowser.removeCurrentTab();
  await visible;
  ok(true, "Visible event received");

  let willNavigate = once(target, "will-navigate");
  let navigate = once(target, "navigate");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.location = "data:text/html,<meta charset='utf8'/>test navigation";
  });
  await willNavigate;
  ok(true, "will-navigate event received");
  await navigate;
  ok(true, "navigate event received");

  let close = once(target, "close");
  gBrowser.removeCurrentTab();
  await close;
  ok(true, "close event received");
});
