/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;

// Services.prefs.setBoolPref("devtools.debugger.log", true);
// SimpleTest.registerCleanupFunction(() => {
//   Services.prefs.clearUserPref("devtools.debugger.log");
// });

let tempScope = {};
Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm", tempScope);
let LayoutHelpers = tempScope.LayoutHelpers;

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", tempScope);
let TargetFactory = devtools.TargetFactory;

Components.utils.import("resource://gre/modules/devtools/Console.jsm", tempScope);
let console = tempScope.console;

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

SimpleTest.registerCleanupFunction(() => {
  console.error("Here we are\n")
  let {DebuggerServer} = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
  console.error("DebuggerServer open connections: " + Object.getOwnPropertyNames(DebuggerServer._connections).length);
});

function openInspector(callback)
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    callback(toolbox.getCurrentPanel(), toolbox);
  }).then(null, console.error);
}

function getActiveInspector()
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.getToolbox(target).getPanel("inspector");
}

function getNodeFront(node)
{
  let inspector = getActiveInspector();
  return inspector.walker.frontForRawNode(node);
}

function getHighlighter()
{
  return gBrowser.selectedBrowser.parentNode.querySelector(".highlighter-container");
}

function getHighlighterOutline()
{
  let h = getHighlighter();
  if (h) {
    return h.querySelector(".highlighter-outline");
  }
}

function getHighlighterOutlineRect() {
  let helper = new LayoutHelpers(window.content);
  let outline = getHighlighterOutline();

  if (outline) {
    let browserOffsetRect = helper.getDirtyRect(gBrowser.selectedBrowser);
    let outlineRect = helper.getDirtyRect(outline);
    outlineRect.top -= browserOffsetRect.top;
    outlineRect.left -= browserOffsetRect.left;

    return outlineRect;
  }
}

function isHighlighting()
{
  let outline = getHighlighterOutline();
  return outline && !outline.hasAttribute("hidden");
}

function getHighlitNode()
{
  if (isHighlighting()) {
    let helper = new LayoutHelpers(window.content);
    let outlineRect = getHighlighterOutlineRect();

    let a = {
      x: outlineRect.left,
      y: outlineRect.top
    };

    let b = {
      x: a.x + outlineRect.width,
      y: a.y + outlineRect.height
    };

    let {x, y} = getMidPoint(a, b);
    return helper.getElementFromPoint(window.content.document, x, y);
  }
}

function getMidPoint(aPointA, aPointB)
{
  let pointC = {};
  pointC.x = (aPointB.x - aPointA.x) / 2 + aPointA.x;
  pointC.y = (aPointB.y - aPointA.y) / 2 + aPointA.y;
  return pointC;
}

function computedView()
{
  let sidebar = getActiveInspector().sidebar;
  let iframe = sidebar.tabbox.querySelector(".iframe-computedview");
  return iframe.contentWindow.computedView;
}

function computedViewTree()
{
  return computedView().view;
}

function ruleView()
{
  let sidebar = getActiveInspector().sidebar;
  let iframe = sidebar.tabbox.querySelector(".iframe-ruleview");
  return iframe.contentWindow.ruleView;
}

function getComputedView() {
  let inspector = getActiveInspector();
  return inspector.sidebar.getWindowForTab("computedview").computedview.view;
}

function synthesizeKeyFromKeyTag(aKeyId) {
  let key = document.getElementById(aKeyId);
  isnot(key, null, "Successfully retrieved the <key> node");

  let modifiersAttr = key.getAttribute("modifiers");

  let name = null;

  if (key.getAttribute("keycode"))
    name = key.getAttribute("keycode");
  else if (key.getAttribute("key"))
    name = key.getAttribute("key");

  isnot(name, null, "Successfully retrieved keycode/key");

  let modifiers = {
    shiftKey: modifiersAttr.match("shift"),
    ctrlKey: modifiersAttr.match("ctrl"),
    altKey: modifiersAttr.match("alt"),
    metaKey: modifiersAttr.match("meta"),
    accelKey: modifiersAttr.match("accel")
  }

  EventUtils.synthesizeKey(name, modifiers);
}

function focusSearchBoxUsingShortcut(panelWin, callback) {
  panelWin.focus();
  let key = panelWin.document.getElementById("nodeSearchKey");
  isnot(key, null, "Successfully retrieved the <key> node");

  let modifiersAttr = key.getAttribute("modifiers");

  let name = null;

  if (key.getAttribute("keycode")) {
    name = key.getAttribute("keycode");
  } else if (key.getAttribute("key")) {
    name = key.getAttribute("key");
  }

  isnot(name, null, "Successfully retrieved keycode/key");

  let modifiers = {
    shiftKey: modifiersAttr.match("shift"),
    ctrlKey: modifiersAttr.match("ctrl"),
    altKey: modifiersAttr.match("alt"),
    metaKey: modifiersAttr.match("meta"),
    accelKey: modifiersAttr.match("accel")
  }

  let searchBox = panelWin.document.getElementById("inspector-searchbox");
  searchBox.addEventListener("focus", function onFocus() {
    searchBox.removeEventListener("focus", onFocus, false);
    callback && callback();
  }, false);
  EventUtils.synthesizeKey(name, modifiers);
}

function getComputedPropertyValue(aName)
{
  let computedview = getComputedView();
  let props = computedview.styleDocument.querySelectorAll(".property-view");

  for (let prop of props) {
    let name = prop.querySelector(".property-name");

    if (name.textContent === aName) {
      let value = prop.querySelector(".property-value");
      return value.textContent;
    }
  }
}

function getContainerForRawNode(markupView, rawNode)
{
  let front = markupView.walker.frontForRawNode(rawNode);
  let container = markupView.getContainer(front);
  return container;
}

SimpleTest.registerCleanupFunction(function () {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.closeToolbox(target);
});
