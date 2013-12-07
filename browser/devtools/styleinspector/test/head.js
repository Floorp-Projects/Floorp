/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

let tempScope = {};

Cu.import("resource:///modules/devtools/gDevTools.jsm", tempScope);
let ConsoleUtils = tempScope.ConsoleUtils;
let gDevTools = tempScope.gDevTools;

Cu.import("resource://gre/modules/devtools/Loader.jsm", tempScope);
let devtools = tempScope.devtools;

let TargetFactory = devtools.TargetFactory;
let {CssHtmlTree} = devtools.require("devtools/styleinspector/computed-view");
let {CssRuleView, _ElementStyle} = devtools.require("devtools/styleinspector/rule-view");
let {CssLogic, CssSelector} = devtools.require("devtools/styleinspector/css-logic");

let promise = devtools.require("sdk/core/promise");

let {
  editableField,
  getInplaceEditorForSpan: inplaceEditor
} = devtools.require("devtools/shared/inplace-editor");
Components.utils.import("resource://gre/modules/devtools/Console.jsm", tempScope);
let console = tempScope.console;

let browser, hudId, hud, hudBox, filterBox, outputNode, cs;

function addTab(aURL)
{
  gBrowser.selectedTab = gBrowser.addTab();
  content.location = aURL;
  browser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
}

function openInspector(callback)
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    callback(toolbox.getCurrentPanel());
  });
}

function getActiveInspector()
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.getToolbox(target).getPanel("inspector");
}

function openRuleView(callback)
{
  openInspector(inspector => {
    inspector.sidebar.once("ruleview-ready", () => {
      inspector.sidebar.select("ruleview");
      let ruleView = inspector.sidebar.getWindowForTab("ruleview").ruleview.view;
      callback(inspector, ruleView);
    })
  });
}

function openComputedView(callback)
{
  openInspector(inspector => {
    inspector.sidebar.once("computedview-ready", () => {
      inspector.sidebar.select("computedview");
      let computedView = inspector.sidebar.getWindowForTab("computedview").computedview.view;
      callback(inspector, computedView);
    })
  });
}

function addStyle(aDocument, aString)
{
  let node = aDocument.createElement('style');
  node.setAttribute("type", "text/css");
  node.textContent = aString;
  aDocument.getElementsByTagName("head")[0].appendChild(node);
  return node;
}

function finishTest()
{
  finish();
}

function tearDown()
{
  try {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.closeToolbox(target);
  }
  catch (ex) {
    dump(ex);
  }
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
  browser = hudId = hud = filterBox = outputNode = cs = null;
}

function getComputedView() {
  let inspector = getActiveInspector();
  return inspector.sidebar.getWindowForTab("computedview").computedview.view;
}

function ruleView()
{
  let inspector = getActiveInspector();
  return inspector.sidebar.getWindowForTab("ruleview").ruleview.view;
}

function waitForEditorFocus(aParent, aCallback)
{
  aParent.addEventListener("focus", function onFocus(evt) {
    if (inplaceEditor(evt.target) && evt.target.tagName == "input") {
      aParent.removeEventListener("focus", onFocus, true);
      let editor = inplaceEditor(evt.target);
      executeSoon(function() {
        aCallback(editor);
      });
    }
  }, true);
}

function waitForEditorBlur(aEditor, aCallback)
{
  let input = aEditor.input;
  input.addEventListener("blur", function onBlur() {
    input.removeEventListener("blur", onBlur, false);
    executeSoon(function() {
      aCallback();
    });
  }, false);
}

function fireCopyEvent(element) {
  let evt = element.ownerDocument.createEvent("Event");
  evt.initEvent("copy", true, true);
  element.dispatchEvent(evt);
}

function contextMenuClick(element) {
  var evt = element.ownerDocument.createEvent('MouseEvents');

  var button = 2;  // right click

  evt.initMouseEvent('contextmenu', true, true,
       element.ownerDocument.defaultView, 1, 0, 0, 0, 0, false,
       false, false, false, button, null);

  element.dispatchEvent(evt);
}

function expectRuleChange(rule) {
  return rule._applyingModifications;
}

function promiseDone(promise) {
  promise.then(null, err => {
    ok(false, "Promise failed: " + err);
    if (err.stack) {
      dump(err.stack);
    }
    SimpleTest.finish();
  });
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

registerCleanupFunction(tearDown);

waitForExplicitFinish();

