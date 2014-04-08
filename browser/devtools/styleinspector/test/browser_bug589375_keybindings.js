/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the key bindings work properly.

let doc;
let inspector;
let computedView;
let iframe;

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    '.matches {color: #F00;}</style>' +
    '<span class="matches">Some styled text</span>' +
    '</div>';
  doc.title = "Style Inspector key binding test";

  openInspector(openComputedView);
}

function openComputedView(aInspector)
{
  inspector = aInspector;
  iframe = inspector._toolbox.frame;

  Services.obs.addObserver(runTests, "StyleInspector-populated", false);

  inspector.sidebar.select("computedview");
}

function runTests()
{
  Services.obs.removeObserver(runTests, "StyleInspector-populated");
  computedView = getComputedView();

  var span = doc.querySelector(".matches");
  ok(span, "captain, we have the matches span");

  inspector.selection.setNode(span);

  is(span, computedView.viewedElement,
    "style inspector node matches the selected node");
  is(computedView.viewedElement, computedView.cssLogic.viewedElement,
     "cssLogic node matches the cssHtmlTree node");

  info("checking keybindings");

  let searchbar = computedView.searchField;
  let propView = getFirstVisiblePropertyView();
  let rulesTable = propView.matchedSelectorsContainer;
  let matchedExpander = propView.matchedExpander;

  info("Adding focus event handler to search filter");
  searchbar.addEventListener("focus", function searchbarFocused() {
    searchbar.removeEventListener("focus", searchbarFocused);
    info("search filter is focused");
    info("tabbing to property expander node");
    EventUtils.synthesizeKey("VK_TAB", {}, iframe.contentWindow);
  });

  info("Adding focus event handler to property expander");
  matchedExpander.addEventListener("focus", function expanderFocused() {
    matchedExpander.removeEventListener("focus", expanderFocused);
    info("property expander is focused");
    info("checking expand / collapse");
    testKey(iframe.contentWindow, "VK_SPACE", rulesTable);
    testKey(iframe.contentWindow, "VK_RETURN", rulesTable);

    checkHelpLinkKeybinding();
    computedView.destroy();
    finishUp();
  });

  info("Making sure that the style inspector panel is focused");
  SimpleTest.waitForFocus(function windowFocused() {
    info("window is focused");
    info("focusing search filter");
    searchbar.focus();
  }, iframe.contentWindow);
}

function getFirstVisiblePropertyView()
{
  let propView = null;
  computedView.propertyViews.some(function(aPropView) {
    if (aPropView.visible) {
      propView = aPropView;
      return true;
    }
    return false;
  });

  return propView;
}

function testKey(aContext, aVirtKey, aRulesTable)
{
  info("testing " + aVirtKey + " key");
  info("expanding rules table");
  EventUtils.synthesizeKey(aVirtKey, {}, aContext);
  isnot(aRulesTable.innerHTML, "", "rules Table is populated");
  info("collapsing rules table");
  EventUtils.synthesizeKey(aVirtKey, {}, aContext);
  is(aRulesTable.innerHTML, "", "rules Table is not populated");
}

function checkHelpLinkKeybinding()
{
  info("checking help link keybinding");
  let propView = getFirstVisiblePropertyView();

  info("check that MDN link is opened on \"F1\"");
  let linkClicked = false;
  propView.mdnLinkClick = function(aEvent) {
    linkClicked = true;
  };
  EventUtils.synthesizeKey("VK_F1", {}, iframe.contentWindow);
  is(linkClicked, true, "MDN link will be shown");
}

function finishUp()
{
  doc = inspector = iframe = computedView = null;
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

  content.location = "data:text/html,default styles test";
}
