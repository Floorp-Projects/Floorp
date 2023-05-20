/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTCASE_URI = TEST_BASE_HTTPS + "media-rules.html";

const PREF_SHOW_AT_RULES_SIDEBAR = "devtools.styleeditor.showAtRulesSidebar";
const PREF_SIDEBAR_WIDTH = "devtools.styleeditor.atRulesSidebarWidth";
const PREF_NAV_WIDTH = "devtools.styleeditor.navSidebarWidth";

// Initial widths for the navigation and media sidebars, which will be set via
// the corresponding preferences.
// The widths should remain between the current min-width and max-width for the
// styleeditor sidebars (currently 100px and 400px).
const NAV_WIDTH = 210;
const MEDIA_WIDTH = 250;

// Test that sidebar in the styleeditor can be resized.
add_task(async function () {
  await pushPref(PREF_SHOW_AT_RULES_SIDEBAR, true);
  await pushPref(PREF_NAV_WIDTH, NAV_WIDTH);
  await pushPref(PREF_SIDEBAR_WIDTH, MEDIA_WIDTH);

  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);
  const doc = panel.panelWindow.document;

  info("Open editor for inline sheet with @media rules to have both splitters");
  const inlineMediaEditor = ui.editors[3];
  inlineMediaEditor.summary.querySelector(".stylesheet-name").click();
  await inlineMediaEditor.getSourceEditor();

  info("Check the initial widths of side panels match the preferences values");
  const navSidebar = doc.querySelector(".splitview-controller");
  is(navSidebar.clientWidth, NAV_WIDTH);

  const mediaSidebar = doc.querySelector(
    ".splitview-active .stylesheet-sidebar"
  );
  is(mediaSidebar.clientWidth, MEDIA_WIDTH);

  info(
    "Resize the navigation splitter and check the navigation sidebar is updated"
  );
  const navSplitter = doc.querySelector(".devtools-side-splitter");
  dragElement(navSplitter, { startX: 1, startY: 10, deltaX: 50, deltaY: 0 });
  is(navSidebar.clientWidth, NAV_WIDTH + 50);

  info("Resize the media splitter and check the media sidebar is updated");
  const mediaSplitter = doc.querySelector(
    ".splitview-active .devtools-side-splitter"
  );
  dragElement(mediaSplitter, { startX: 1, startY: 10, deltaX: -50, deltaY: 0 });
  is(mediaSidebar.clientWidth, MEDIA_WIDTH + 50);
});

/* Helpers */

function dragElement(el, { startX, startY, deltaX, deltaY }) {
  const win = el.ownerGlobal;
  const endX = startX + deltaX;
  const endY = startY + deltaY;

  EventUtils.synthesizeMouse(el, startX, startY, { type: "mousedown" }, win);
  EventUtils.synthesizeMouse(el, endX, endY, { type: "mousemove" }, win);
  EventUtils.synthesizeMouse(el, endX, endY, { type: "mouseup" }, win);
}
