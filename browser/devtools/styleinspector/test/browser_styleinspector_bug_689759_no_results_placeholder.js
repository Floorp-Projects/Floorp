/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the no results placeholder works properly.

let doc;
let inspector;
let computedView;

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    '.matches {color: #F00;}</style>' +
    '<span id="matches" class="matches">Some styled text</span>';
  doc.title = "Tests that the no results placeholder works properly";

  openComputedView(runStyleInspectorTests);
}

function runStyleInspectorTests(aInspector, aComputedView)
{
  inspector = aInspector;
  computedView = aComputedView;

  let span = doc.querySelector("#matches");
  ok(span, "captain, we have the matches span");

  inspector.selection.setNode(span);
  inspector.once("inspector-updated", () => {
    is(span, computedView.viewedElement.rawNode(),
      "style inspector node matches the selected node");
    SI_AddFilterText();
  });

}

function SI_AddFilterText()
{
  let searchbar = computedView.searchField;
  let searchTerm = "xxxxx";

  inspector.once("computed-view-refreshed", SI_checkPlaceholderVisible);

  info("setting filter text to \"" + searchTerm + "\"");
  searchbar.focus();
  for each (let c in searchTerm) {
    EventUtils.synthesizeKey(c, {}, computedView.styleWindow);
  }
}

function SI_checkPlaceholderVisible()
{
  info("SI_checkPlaceholderVisible called");
  let placeholder = computedView.noResults;
  let win = computedView.styleWindow;
  let display = win.getComputedStyle(placeholder).display;

  is(display, "block", "placeholder is visible");

  SI_ClearFilterText();
}

function SI_ClearFilterText()
{
  let searchbar = computedView.searchField;

  inspector.once("computed-view-refreshed", SI_checkPlaceholderHidden);
  info("clearing filter text");
  searchbar.focus();
  searchbar.value = "";
  EventUtils.synthesizeKey("c", {}, computedView.styleWindow);
}

function SI_checkPlaceholderHidden()
{
  let placeholder = computedView.noResults;
  let win = computedView.styleWindow;
  let display = win.getComputedStyle(placeholder).display;

  is(display, "none", "placeholder is hidden");

  finishUp();
}

function finishUp()
{
  doc = inspector = computedView = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,no results placeholder test";
}
