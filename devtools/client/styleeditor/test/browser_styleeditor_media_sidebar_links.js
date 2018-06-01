/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Tests responsive mode links for
 * @media sidebar width and height related conditions */

const asyncStorage = require("devtools/shared/async-storage");
Services.prefs.setCharPref("devtools.devices.url",
  "http://example.com/browser/devtools/client/responsive.html/test/browser/devices.json");

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.devices.url");
  asyncStorage.removeItem("devtools.devices.url_cache");
});

loader.lazyRequireGetter(this, "ResponsiveUIManager", "devtools/client/responsive.html/manager", true);

const TESTCASE_URI = TEST_BASE_HTTPS + "media-rules.html";
const responsiveModeToggleClass = ".media-responsive-mode-toggle";

add_task(async function() {
  const {ui} = await openStyleEditorForURL(TESTCASE_URI);

  const editor = ui.editors[1];
  await openEditor(editor);

  const tab = gBrowser.selectedTab;
  testNumberOfLinks(editor);
  await testMediaLink(editor, tab, ui, 2, "width", 400);
  await testMediaLink(editor, tab, ui, 3, "height", 300);

  await closeRDM(tab, ui);
  doFinalChecks(editor);
});

function testNumberOfLinks(editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  const conditions = sidebar.querySelectorAll(".media-rule-condition");

  info("Testing if media rules have the appropriate number of links");
  ok(!conditions[0].querySelector(responsiveModeToggleClass),
    "There should be no links in the first media rule.");
  ok(!conditions[1].querySelector(responsiveModeToggleClass),
     "There should be no links in the second media rule.");
  ok(conditions[2].querySelector(responsiveModeToggleClass),
     "There should be 1 responsive mode link in the media rule");
  is(conditions[3].querySelectorAll(responsiveModeToggleClass).length, 2,
       "There should be 2 responsive mode links in the media rule");
}

async function testMediaLink(editor, tab, ui, itemIndex, type, value) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  let conditions = sidebar.querySelectorAll(".media-rule-condition");

  const onMediaChange = once(ui, "media-list-changed");

  info("Launching responsive mode");
  conditions[itemIndex].querySelector(responsiveModeToggleClass).click();

  const rdmUI = ResponsiveUIManager.getResponsiveUIForTab(tab);
  const onContentResize = waitForResizeTo(rdmUI, type, value);
  rdmUI.transitionsEnabled = false;

  info("Waiting for the @media list to update");
  await onMediaChange;
  await onContentResize;

  ok(ResponsiveUIManager.isActiveForTab(tab),
    "Responsive mode should be active.");
  conditions = sidebar.querySelectorAll(".media-rule-condition");
  ok(!conditions[itemIndex].classList.contains("media-condition-unmatched"),
     "media rule should now be matched after responsive mode is active");

  const dimension = (await getSizing(rdmUI))[type];
  is(dimension, value, `${type} should be properly set.`);
}

async function closeRDM(tab, ui) {
  info("Closing responsive mode");
  ResponsiveUIManager.toggle(window, tab);
  const onMediaChange = waitForNEvents(ui, "media-list-changed", 2);
  await once(ResponsiveUIManager, "off");
  await onMediaChange;
  ok(!ResponsiveUIManager.isActiveForTab(tab),
     "Responsive mode should no longer be active.");
}

function doFinalChecks(editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  let conditions = sidebar.querySelectorAll(".media-rule-condition");
  conditions = sidebar.querySelectorAll(".media-rule-condition");
  ok(conditions[2].classList.contains("media-condition-unmatched"),
     "The width condition should now be unmatched");
  ok(conditions[3].classList.contains("media-condition-unmatched"),
     "The height condition should now be unmatched");
}

/* Helpers */
function waitForResizeTo(rdmUI, type, value) {
  return new Promise(resolve => {
    const onResize = data => {
      if (data[type] != value) {
        return;
      }
      rdmUI.off("content-resize", onResize);
      info(`Got content-resize to a ${type} of ${value}`);
      resolve();
    };
    info(`Waiting for content-resize to a ${type} of ${value}`);
    rdmUI.on("content-resize", onResize);
  });
}

async function getSizing(rdmUI) {
  const browser = rdmUI.getViewportBrowser();
  const sizing = await ContentTask.spawn(browser, {}, async function() {
    return {
      width: content.innerWidth,
      height: content.innerHeight
    };
  });
  return sizing;
}

function openEditor(editor) {
  getLinkFor(editor).click();

  return editor.getSourceEditor();
}

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}
