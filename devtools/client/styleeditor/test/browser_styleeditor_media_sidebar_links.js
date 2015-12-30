/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests responsive mode links for
 * @media sidebar width and height related conditions */

const {ResponsiveUIManager} =
      Cu.import("resource://devtools/client/responsivedesign/responsivedesign.jsm", {});
const TESTCASE_URI = TEST_BASE_HTTPS + "media-rules.html";

waitForExplicitFinish();

add_task(function*() {
  let {ui} = yield openStyleEditorForURL(TESTCASE_URI);

  let mediaEditor = ui.editors[1];
  yield openEditor(mediaEditor);

  yield testLinkifiedConditions(mediaEditor, gBrowser.selectedTab, ui);
});

function testLinkifiedConditions(editor, tab, ui) {
  let sidebar = editor.details.querySelector(".stylesheet-sidebar");
  let conditions = sidebar.querySelectorAll(".media-rule-condition");
  let responsiveModeToggleClass = ".media-responsive-mode-toggle";

  info("Testing if media rules have the appropriate number of links");
  ok(!conditions[0].querySelector(responsiveModeToggleClass),
    "There should be no links in the first media rule.");
  ok(!conditions[1].querySelector(responsiveModeToggleClass),
    "There should be no links in the second media rule.")
  ok(conditions[2].querySelector(responsiveModeToggleClass),
     "There should be 1 responsive mode link in the media rule");
  is(conditions[3].querySelectorAll(responsiveModeToggleClass).length, 2,
       "There should be 2 resposnive mode links in the media rule");

  info("Launching responsive mode");
  conditions[2].querySelector(responsiveModeToggleClass).click();

  info("Waiting for the @media list to update");
  let onMediaChange = once("media-list-changed", ui);
  yield once("on", ResponsiveUIManager);
  yield onMediaChange;

  ok(ResponsiveUIManager.isActiveForTab(tab),
    "Responsive mode should be active.");
  conditions = sidebar.querySelectorAll(".media-rule-condition");
  ok(!conditions[2].classList.contains("media-condition-unmatched"),
     "media rule should now be matched after responsive mode is active");

  info("Closing responsive mode");
  ResponsiveUIManager.toggle(window, tab);
  onMediaChange = once("media-list-changed", ui);
  yield once("off", ResponsiveUIManager);
  yield onMediaChange;

  ok(!ResponsiveUIManager.isActiveForTab(tab),
     "Responsive mode should no longer be active.");
  conditions = sidebar.querySelectorAll(".media-rule-condition");
  ok(conditions[2].classList.contains("media-condition-unmatched"),
       "media rule should now be unmatched after responsive mode is closed");
}

/* Helpers */
function once(event, target) {
  let deferred = promise.defer();
  target.once(event, () => {
    deferred.resolve();
  });
  return deferred.promise;
}

function openEditor(editor) {
  getLinkFor(editor).click();

  return editor.getSourceEditor();
}

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}
