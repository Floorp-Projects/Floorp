/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the key bindings work properly.

let doc;
let stylePanel;

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    '.matches {color: #F00;}</style>' +
    '<span class="matches">Some styled text</span>' +
    '</div>';
  doc.title = "Style Inspector key binding test";
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

  Services.obs.addObserver(SI_test, "StyleInspector-populated", false);
  SI_inspectNode();
}

function SI_inspectNode()
{
  var span = doc.querySelector(".matches");
  ok(span, "captain, we have the matches span");

  let htmlTree = stylePanel.cssHtmlTree;
  stylePanel.selectNode(span);

  is(span, htmlTree.viewedElement,
    "style inspector node matches the selected node");
  is(htmlTree.viewedElement, stylePanel.cssLogic.viewedElement,
     "cssLogic node matches the cssHtmlTree node");
}

function SI_test()
{
  Services.obs.removeObserver(SI_test, "StyleInspector-populated", false);

  info("checking keybindings");

  let iframe = stylePanel.iframe;
  let searchbar = stylePanel.cssHtmlTree.searchField;
  let propView = getFirstVisiblePropertyView();
  let rulesTable = propView.matchedSelectorsContainer;
  let matchedExpander = propView.matchedExpander;

  info("Adding focus event handler to property expander");
  matchedExpander.addEventListener("focus", function expanderFocused() {
    this.removeEventListener("focus", expanderFocused);
    info("property expander is focused");
    info("checking expand / collapse");
    testKey(iframe.contentWindow, "VK_SPACE", rulesTable);
    testKey(iframe.contentWindow, "VK_RETURN", rulesTable);

    checkHelpLinkKeybinding();
    Services.obs.addObserver(finishUp, "StyleInspector-closed", false);
    stylePanel.close();
  });

  info("Adding focus event handler to search filter");
  searchbar.addEventListener("focus", function searchbarFocused() {
    this.removeEventListener("focus", searchbarFocused);
    info("search filter is focused");
    info("tabbing to property expander node");
    EventUtils.synthesizeKey("VK_TAB", {}, iframe.contentWindow);
  });

  info("Making sure that the style inspector panel is focused");
  SimpleTest.waitForFocus(function windowFocused() {
    info("window is focused");
    info("focusing search filter");
    searchbar.focus();
  }, stylePanel.iframe.contentWindow);
}

function getFirstVisiblePropertyView()
{
  let propView = null;
  stylePanel.cssHtmlTree.propertyViews.some(function(aPropView) {
    if (aPropView.visible) {
      propView = aPropView;
      return true;
    }
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
  let iframe = stylePanel.iframe;
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

  content.location = "data:text/html,default styles test";
}
