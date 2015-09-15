/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Common code shared by browser_toolbox_options_disable_cache-*.js
const TEST_URI = URL_ROOT + "browser_toolbox_options_disable_cache.sjs";
var tabs = [
{
  title: "Tab 0",
  desc: "Toggles cache on.",
  startToolbox: true
},
{
  title: "Tab 1",
  desc: "Toolbox open before Tab 1 toggles cache.",
  startToolbox: true
},
{
  title: "Tab 2",
  desc: "Opens toolbox after Tab 1 has toggled cache. Also closes and opens.",
  startToolbox: false
},
{
  title: "Tab 3",
  desc: "No toolbox",
  startToolbox: false
}];

function* initTab(tabX, startToolbox) {
  tabX.tab = yield addTab(TEST_URI);
  tabX.target = TargetFactory.forTab(tabX.tab);

  if (startToolbox) {
    tabX.toolbox = yield gDevTools.showToolbox(tabX.target, "options");
  }
}

function* checkCacheStateForAllTabs(states) {
  for (let i = 0; i < tabs.length; i ++) {
    let tab = tabs[i];
    yield checkCacheEnabled(tab, states[i]);
  }
}

function* checkCacheEnabled(tabX, expected) {
  gBrowser.selectedTab = tabX.tab;

  yield reloadTab(tabX);

  let doc = content.document;
  let h1 = doc.querySelector("h1");
  let oldGuid = h1.textContent;

  yield reloadTab(tabX);

  doc = content.document;
  h1 = doc.querySelector("h1");
  let guid = h1.textContent;

  if (expected) {
    is(guid, oldGuid, tabX.title + " cache is enabled");
  } else {
    isnot(guid, oldGuid, tabX.title + " cache is not enabled");
  }
}

function* setDisableCacheCheckboxChecked(tabX, state) {
  gBrowser.selectedTab = tabX.tab;

  let panel = tabX.toolbox.getCurrentPanel();
  let cbx = panel.panelDoc.getElementById("devtools-disable-cache");

  cbx.scrollIntoView();

  // After uising scrollIntoView() we need to wait for the browser to scroll.
  yield waitForTick();

  if (cbx.checked !== state) {
    info("Setting disable cache checkbox to " + state + " for " + tabX.title);
    EventUtils.synthesizeMouseAtCenter(cbx, {}, panel.panelWin);

    // We need to wait for all checkboxes to be updated and the docshells to
    // apply the new cache settings.
    yield waitForTick();
  }
}

function reloadTab(tabX) {
  let def = promise.defer();
  let browser = gBrowser.selectedBrowser;

  // once() doesn't work here so we use a standard handler instead.
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    info("Reloaded tab " + tabX.title);
    def.resolve();
  }, true);

  info("Reloading tab " + tabX.title);
  let mm = getFrameScript();
  mm.sendAsyncMessage("devtools:test:reload");

  return def.promise;
}

function* destroyTab(tabX) {
  let toolbox = gDevTools.getToolbox(tabX.target);

  let onceDestroyed = promise.resolve();
  if (toolbox) {
    onceDestroyed = gDevTools.once("toolbox-destroyed");
  }

  info("Removing tab " + tabX.title);
  gBrowser.removeTab(tabX.tab);
  info("Removed tab " + tabX.title);

  info("Waiting for toolbox-destroyed");
  yield onceDestroyed;
}

function* finishUp() {
  for (let tab of tabs) {
    yield destroyTab(tab);
  }

  tabs = null;
}
