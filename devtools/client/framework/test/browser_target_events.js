/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var target;

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(onLoad);
}

function onLoad() {
  target = TargetFactory.forTab(gBrowser.selectedTab);

  is(target.tab, gBrowser.selectedTab, "Target linked to the right tab.");

  target.once("hidden", onHidden);
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
}

function onHidden() {
  ok(true, "Hidden event received");
  target.once("visible", onVisible);
  gBrowser.removeCurrentTab();
}

function onVisible() {
  ok(true, "Visible event received");
  target.once("will-navigate", onWillNavigate);

  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.location = "data:text/html,<meta charset='utf8'/>test navigation";
  });
}

async function onWillNavigate() {
  ok(true, "will-navigate event received");
  target.on("navigate", onNavigate);
}

function onNavigate() {
  ok(true, "navigate event received");
  target.off("navigate", onNavigate);
  target.once("close", onClose);
  gBrowser.removeCurrentTab();
}

function onClose() {
  ok(true, "close event received");

  target = null;
  finish();
}
