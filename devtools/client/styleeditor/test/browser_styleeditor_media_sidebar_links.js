/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Tests responsive mode links for
 * @media sidebar width and height related conditions */

loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "resource://devtools/client/responsive/manager.js"
);

const TESTCASE_URI = TEST_BASE_HTTPS + "media-rules.html";
const responsiveModeToggleClass = ".media-responsive-mode-toggle";

add_task(async function () {
  // ensure that original RDM size is big enough so it doesn't match the
  // media queries by default.
  await pushPref("devtools.responsive.viewport.width", 1000);
  await pushPref("devtools.responsive.viewport.height", 1000);

  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  const editor = ui.editors[1];
  await openEditor(editor);

  const tab = gBrowser.selectedTab;
  testNumberOfLinks(editor);
  await testMediaLink(editor, tab, ui, 2, "width", 550);
  await testMediaLink(editor, tab, ui, 3, "height", 300);

  const onMediaChange = waitForManyEvents(ui, 1000);
  await closeRDM(tab);

  info("Wait for at-rules-list-changed events to settle on StyleEditorUI");
  await onMediaChange;
  doFinalChecks(editor);
});

function testNumberOfLinks(editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  const conditions = sidebar.querySelectorAll(".at-rule-condition");

  info("Testing if media rules have the appropriate number of links");
  ok(
    !conditions[0].querySelector(responsiveModeToggleClass),
    "There should be no links in the first media rule."
  );
  ok(
    !conditions[1].querySelector(responsiveModeToggleClass),
    "There should be no links in the second media rule."
  );
  ok(
    conditions[2].querySelector(responsiveModeToggleClass),
    "There should be 1 responsive mode link in the media rule"
  );
  is(
    conditions[3].querySelectorAll(responsiveModeToggleClass).length,
    2,
    "There should be 2 responsive mode links in the media rule"
  );
}

async function testMediaLink(editor, tab, ui, itemIndex, type, value) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  const conditions = sidebar.querySelectorAll(".at-rule-condition");
  const onRDMOpened = once(ui, "responsive-mode-opened");

  ok(
    sidebar
      .querySelectorAll(".at-rule-condition")
      [itemIndex].classList.contains("media-condition-unmatched"),
    `The ${type} condition is not matched when not in responsive mode`
  );

  info("Launching responsive mode");
  conditions[itemIndex].querySelector(responsiveModeToggleClass).click();
  await onRDMOpened;
  const rdmUI = ResponsiveUIManager.getResponsiveUIForTab(tab);
  await waitForResizeTo(rdmUI, type, value);
  rdmUI.transitionsEnabled = false;

  info("Wait for RDM ui to be fully loaded");
  await waitForRDMLoaded(rdmUI);

  // Ensure that the content has reflowed, which will ensure that all the
  // element classes are reported correctly.
  await promiseContentReflow(rdmUI);

  ok(
    ResponsiveUIManager.isActiveForTab(tab),
    "Responsive mode should be active."
  );
  await waitFor(() => {
    const el = sidebar.querySelectorAll(".at-rule-condition")[itemIndex];
    return !el.classList.contains("media-condition-unmatched");
  });
  ok(true, "media rule should now be matched after responsive mode is active");

  const dimension = (await getSizing(rdmUI))[type];
  is(dimension, value, `${type} should be properly set.`);
}

function doFinalChecks(editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  let conditions = sidebar.querySelectorAll(".at-rule-condition");
  conditions = sidebar.querySelectorAll(".at-rule-condition");
  ok(
    conditions[2].classList.contains("media-condition-unmatched"),
    "The width condition should now be unmatched"
  );
  ok(
    conditions[3].classList.contains("media-condition-unmatched"),
    "The height condition should now be unmatched"
  );
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

function promiseContentReflow(ui) {
  return SpecialPowers.spawn(ui.getViewportBrowser(), [], async function () {
    return new Promise(resolve => {
      content.window.requestAnimationFrame(() => {
        content.window.requestAnimationFrame(resolve);
      });
    });
  });
}

async function getSizing(rdmUI) {
  const browser = rdmUI.getViewportBrowser();
  const sizing = await SpecialPowers.spawn(browser, [], async function () {
    return {
      width: content.innerWidth,
      height: content.innerHeight,
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
