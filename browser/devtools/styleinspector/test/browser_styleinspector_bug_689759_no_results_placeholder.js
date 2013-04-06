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

  openInspector(openComputedView);
}

function openComputedView(aInspector)
{
  inspector = aInspector;

  inspector.sidebar.once("computedview-ready", function() {
    inspector.sidebar.select("computedview");
    computedView = getComputedView(inspector);

    runStyleInspectorTests();
  });
}


function runStyleInspectorTests()
{
  Services.obs.addObserver(SI_AddFilterText, "StyleInspector-populated", false);

  let span = doc.querySelector("#matches");
  ok(span, "captain, we have the matches span");

  inspector.selection.setNode(span);

  is(span, computedView.viewedElement,
    "style inspector node matches the selected node");
  is(computedView.viewedElement, computedView.cssLogic.viewedElement,
     "cssLogic node matches the cssHtmlTree node");
}

function SI_AddFilterText()
{
  Services.obs.removeObserver(SI_AddFilterText, "StyleInspector-populated");

  let searchbar = computedView.searchField;
  let searchTerm = "xxxxx";

  Services.obs.addObserver(SI_checkPlaceholderVisible, "StyleInspector-populated", false);
  info("setting filter text to \"" + searchTerm + "\"");
  searchbar.focus();
  for each (let c in searchTerm) {
    EventUtils.synthesizeKey(c, {}, computedView.styleWindow);
  }
}

function SI_checkPlaceholderVisible()
{
  Services.obs.removeObserver(SI_checkPlaceholderVisible, "StyleInspector-populated");
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

  Services.obs.addObserver(SI_checkPlaceholderHidden, "StyleInspector-populated", false);
  info("clearing filter text");
  searchbar.focus();
  searchbar.value = "";
  EventUtils.synthesizeKey("c", {}, computedView.styleWindow);
}

function SI_checkPlaceholderHidden()
{
  Services.obs.removeObserver(SI_checkPlaceholderHidden, "StyleInspector-populated");
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
