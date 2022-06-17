/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Tests responsive mode links for
 * @media sidebar width and height related conditions */

loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "devtools/client/responsive/manager"
);

const TESTCASE_URI = TEST_BASE_HTTPS + "media-rules.html";
const responsiveModeToggleClass = ".media-responsive-mode-toggle";

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  const editor = ui.editors[1];
  await openEditor(editor);

  const tab = gBrowser.selectedTab;
  testNumberOfLinks(editor);
  await testMediaLink(editor, tab, ui, 2, "width", 550);
  await testMediaLink(editor, tab, ui, 3, "height", 300);

  const onMediaChange = waitForManyEvents(ui, 1000);
  await closeRDM(tab);

  info("Wait for media-list-changed events to settle on StyleEditorUI");
  await onMediaChange;
  doFinalChecks(editor);
});

function testNumberOfLinks(editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  const conditions = sidebar.querySelectorAll(".media-rule-condition");

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
  let conditions = sidebar.querySelectorAll(".media-rule-condition");

  const onMediaChange = once(ui, "media-list-changed");
  const onRDMOpened = once(ui, "responsive-mode-opened");

  info("Launching responsive mode");
  conditions[itemIndex].querySelector(responsiveModeToggleClass).click();
  await onRDMOpened;
  const rdmUI = ResponsiveUIManager.getResponsiveUIForTab(tab);
  await waitForResizeTo(rdmUI, type, value);
  rdmUI.transitionsEnabled = false;

  info("Wait for RDM ui to be fully loaded");
  await waitForRDMLoaded(rdmUI);

  info("Waiting for the @media list to update");
  await onMediaChange;

  // Ensure that the content has reflowed, which will ensure that all the
  // element classes are reported correctly.
  await promiseContentReflow(rdmUI);

  ok(
    ResponsiveUIManager.isActiveForTab(tab),
    "Responsive mode should be active."
  );
  conditions = sidebar.querySelectorAll(".media-rule-condition");
  ok(
    !conditions[itemIndex].classList.contains("media-condition-unmatched"),
    "media rule should now be matched after responsive mode is active"
  );

  const dimension = (await getSizing(rdmUI))[type];
  is(dimension, value, `${type} should be properly set.`);
}

function doFinalChecks(editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  let conditions = sidebar.querySelectorAll(".media-rule-condition");
  conditions = sidebar.querySelectorAll(".media-rule-condition");
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
  return SpecialPowers.spawn(ui.getViewportBrowser(), [], async function() {
    return new Promise(resolve => {
      content.window.requestAnimationFrame(() => {
        content.window.requestAnimationFrame(resolve);
      });
    });
  });
}

async function getSizing(rdmUI) {
  const browser = rdmUI.getViewportBrowser();
  const sizing = await SpecialPowers.spawn(browser, [], async function() {
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
