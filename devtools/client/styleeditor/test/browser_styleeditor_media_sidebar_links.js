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

loader.lazyRequireGetter(this, "ResponsiveUIManager", "devtools/client/responsivedesign/responsivedesign");

const TESTCASE_URI = TEST_BASE_HTTPS + "media-rules.html";
const responsiveModeToggleClass = ".media-responsive-mode-toggle";

add_task(function* () {
  let {ui} = yield openStyleEditorForURL(TESTCASE_URI);

  let editor = ui.editors[1];
  yield openEditor(editor);

  let tab = gBrowser.selectedTab;
  testNumberOfLinks(editor);
  yield testMediaLink(editor, tab, ui, 2, "width", 400);
  yield testMediaLink(editor, tab, ui, 3, "height", 300);

  yield closeRDM(tab, ui);
  doFinalChecks(editor);
});

function testNumberOfLinks(editor) {
  let sidebar = editor.details.querySelector(".stylesheet-sidebar");
  let conditions = sidebar.querySelectorAll(".media-rule-condition");

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

function* testMediaLink(editor, tab, ui, itemIndex, type, value) {
  let sidebar = editor.details.querySelector(".stylesheet-sidebar");
  let conditions = sidebar.querySelectorAll(".media-rule-condition");

  let onMediaChange = once(ui, "media-list-changed");

  info("Launching responsive mode");
  conditions[itemIndex].querySelector(responsiveModeToggleClass).click();

  let rdmUI = ResponsiveUIManager.getResponsiveUIForTab(tab);
  let onContentResize = waitForResizeTo(rdmUI, type, value);
  rdmUI.transitionsEnabled = false;

  info("Waiting for the @media list to update");
  yield onMediaChange;
  yield onContentResize;

  ok(ResponsiveUIManager.isActiveForTab(tab),
    "Responsive mode should be active.");
  conditions = sidebar.querySelectorAll(".media-rule-condition");
  ok(!conditions[itemIndex].classList.contains("media-condition-unmatched"),
     "media rule should now be matched after responsive mode is active");

  let dimension = (yield getSizing(rdmUI))[type];
  is(dimension, value, `${type} should be properly set.`);
}

function* closeRDM(tab, ui) {
  info("Closing responsive mode");
  ResponsiveUIManager.toggle(window, tab);
  let onMediaChange = waitForNEvents(ui, "media-list-changed", 2);
  yield once(ResponsiveUIManager, "off");
  yield onMediaChange;
  ok(!ResponsiveUIManager.isActiveForTab(tab),
     "Responsive mode should no longer be active.");
}

function doFinalChecks(editor) {
  let sidebar = editor.details.querySelector(".stylesheet-sidebar");
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
    let onResize = (_, data) => {
      if (data[type] != value) {
        return;
      }
      ResponsiveUIManager.off("content-resize", onResize);
      if (rdmUI.off) {
        rdmUI.off("content-resize", onResize);
      }
      info(`Got content-resize to a ${type} of ${value}`);
      resolve();
    };
    info(`Waiting for content-resize to a ${type} of ${value}`);
    // Old RDM emits on manager
    ResponsiveUIManager.on("content-resize", onResize);
    // New RDM emits on ui
    if (rdmUI.on) {
      rdmUI.on("content-resize", onResize);
    }
  });
}

function* getSizing(rdmUI) {
  let browser = rdmUI.getViewportBrowser();
  let sizing = yield ContentTask.spawn(browser, {}, function* () {
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
