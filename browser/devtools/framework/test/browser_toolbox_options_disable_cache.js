/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that disabling JavaScript for a tab works as it should.

const TEST_URI = "http://mochi.test:8888/browser/browser/devtools/framework/" +
                 "test/browser_toolbox_options_disable_cache.sjs";

let doc;
let toolbox;

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    gDevTools.showToolbox(target).then(testSelectTool);
  }, true);

  content.location = TEST_URI;
}

function testSelectTool(aToolbox) {
  toolbox = aToolbox;
  toolbox.once("options-selected", testCacheEnabled);
  toolbox.selectTool("options");
}

function testCacheEnabled() {
  let prevTimestamp = getGUID();

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    is(prevTimestamp, getGUID(), "GUID has not changed (page is cached)");

    testCacheEnabled2();
  }, true);

  doc.location.reload(false);
}

function testCacheEnabled2() {
  let prevTimestamp = getGUID();

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    is(prevTimestamp, getGUID(),
       "GUID has not changed after page refresh (page is cached)");

    testCacheDisabled();
  }, true);

  doc.location.reload(false);
}

function testCacheDisabled() {
  let prevTimestamp = getGUID();

  let panel = toolbox.getCurrentPanel();
  let cbx = panel.panelDoc.getElementById("devtools-disable-cache");
  let browser = gBrowser.selectedBrowser;

  browser.addEventListener("load", function onLoad(evt) {
    browser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    isnot(prevTimestamp, getGUID(), "GUID has changed (page is not cached)");
    testCacheDisabled2();
  }, true);

  info("disabling cache");

  cbx.scrollIntoView();

  // After uising scrollIntoView() we need to use executeSoon() to wait for the
  // browser to scroll.
  executeSoon(function() {
    EventUtils.synthesizeMouseAtCenter(cbx, {}, panel.panelWin);
  });
}

function testCacheDisabled2() {
  let prevTimestamp = getGUID();

  let browser = gBrowser.selectedBrowser;

  browser.addEventListener("load", function onLoad(evt) {
    browser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    isnot(prevTimestamp, getGUID(),
       "GUID has changed after page refresh (page is not cached)");
    finishUp();
  }, true);

  doc.location.reload(false);
}

function getGUID() {
  return doc.querySelector("h1").textContent;
}

function finishUp() {
  toolbox.destroy().then(() => {
    gBrowser.removeCurrentTab();
    toolbox = doc = null;
    finish();
  }).then(null, console.error);
}
