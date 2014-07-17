/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that disabling the cache for a tab works as it should.

const TEST_URI = "http://mochi.test:8888/browser/browser/devtools/framework/" +
                 "test/browser_toolbox_options_disable_cache.sjs";
let tabs = [
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

let test = asyncTest(function*() {
  // Initialise tabs: 1 and 2 with a toolbox, 3 and 4 without.
  for (let tab of tabs) {
    yield initTab(tab, tab.startToolbox);
  }

  // Ensure cache is enabled for all tabs.
  yield checkCacheStateForAllTabs([true, true, true, true]);

  // Check the checkbox in tab 0 and ensure cache is disabled for tabs 0 and 1.
  yield setDisableCacheCheckboxChecked(tabs[0], true);
  yield checkCacheStateForAllTabs([false, false, true, true]);

  // Open toolbox in tab 2 and ensure the cache is then disabled.
  tabs[2].toolbox = yield gDevTools.showToolbox(tabs[2].target, "options");
  yield checkCacheEnabled(tabs[2], false);

  // Close toolbox in tab 2 and ensure the cache is enabled again
  yield tabs[2].toolbox.destroy();
  tabs[2].target = TargetFactory.forTab(tabs[2].tab);
  yield checkCacheEnabled(tabs[2], true);

  // Open toolbox in tab 2 and ensure the cache is then disabled.
  tabs[2].toolbox = yield gDevTools.showToolbox(tabs[2].target, "options");
  yield checkCacheEnabled(tabs[2], false);

  // Check the checkbox in tab 2 and ensure cache is enabled for all tabs.
  yield setDisableCacheCheckboxChecked(tabs[2], false);
  yield checkCacheStateForAllTabs([true, true, true, true]);

  yield finishUp();
});

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
  content.document.location.reload(false);

  return def.promise;
}

function* destroyTab(tabX) {
  let toolbox = gDevTools.getToolbox(tabX.target);

  info("Removing tab " + tabX.title);
  gBrowser.removeTab(tabX.tab);
  info("Removed tab " + tabX.title);

  if (toolbox) {
    info("Waiting for toolbox-destroyed");
    yield gDevTools.once("toolbox-destroyed");
    info("toolbox-destroyed event received for " + tabX.title);
  }
}

function* finishUp() {
  for (let tab of tabs) {
    yield destroyTab(tab);
  }

  tabs = null;
}
