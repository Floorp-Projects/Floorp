/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the no results placeholder works properly.

let doc;
let stylePanel;

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    '.matches {color: #F00;}</style>' +
    '<span id="matches" class="matches">Some styled text</span>';
  doc.title = "Tests that the no results placeholder works properly";
  ok(window.StyleInspector, "StyleInspector exists");
  stylePanel = new StyleInspector(window);
  Services.obs.addObserver(runStyleInspectorTests, "StyleInspector-opened", false);
  stylePanel.createPanel(false, function() {
    stylePanel.open(doc.body);
  });
}

function runStyleInspectorTests()
{
  Services.obs.removeObserver(runStyleInspectorTests, "StyleInspector-opened", false);

  ok(stylePanel.isOpen(), "style inspector is open");

  Services.obs.addObserver(SI_AddFilterText, "StyleInspector-populated", false);

  let span = doc.querySelector("#matches");
  ok(span, "captain, we have the matches span");

  let htmlTree = stylePanel.cssHtmlTree;
  stylePanel.selectNode(span);

  is(span, htmlTree.viewedElement,
    "style inspector node matches the selected node");
  is(htmlTree.viewedElement, stylePanel.cssLogic.viewedElement,
     "cssLogic node matches the cssHtmlTree node");
}

function SI_AddFilterText()
{
  Services.obs.removeObserver(SI_AddFilterText, "StyleInspector-populated", false);

  let iframe = stylePanel.iframe;
  let searchbar = stylePanel.cssHtmlTree.searchField;
  let searchTerm = "xxxxx";

  Services.obs.addObserver(SI_checkPlaceholderVisible, "StyleInspector-populated", false);
  info("setting filter text to \"" + searchTerm + "\"");
  searchbar.focus();
  for each (let c in searchTerm) {
    EventUtils.synthesizeKey(c, {}, iframe.contentWindow);
  }
}

function SI_checkPlaceholderVisible()
{
  Services.obs.removeObserver(SI_checkPlaceholderVisible, "StyleInspector-populated", false);
  info("SI_checkPlaceholderVisible called");
  let placeholder = stylePanel.cssHtmlTree.noResults;
  let iframe = stylePanel.iframe;
  let display = iframe.contentWindow.getComputedStyle(placeholder).display;

  is(display, "block", "placeholder is visible");

  SI_ClearFilterText();
}

function SI_ClearFilterText()
{
  let iframe = stylePanel.iframe;
  let searchbar = stylePanel.cssHtmlTree.searchField;

  Services.obs.addObserver(SI_checkPlaceholderHidden, "StyleInspector-populated", false);
  info("clearing filter text");
  searchbar.focus();
  searchbar.value = "";
  EventUtils.synthesizeKey("c", {}, iframe.contentWindow);
}

function SI_checkPlaceholderHidden()
{
  Services.obs.removeObserver(SI_checkPlaceholderHidden, "StyleInspector-populated", false);
  let placeholder = stylePanel.cssHtmlTree.noResults;
  let iframe = stylePanel.iframe;
  let display = iframe.contentWindow.getComputedStyle(placeholder).display;

  is(display, "none", "placeholder is hidden");

  Services.obs.addObserver(finishUp, "StyleInspector-closed", false);
  stylePanel.close();
}

function finishUp()
{
  Services.obs.removeObserver(finishUp, "StyleInspector-closed", false);
  ok(!stylePanel.isOpen(), "style inspector is closed");
  doc = stylePanel = null;
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
