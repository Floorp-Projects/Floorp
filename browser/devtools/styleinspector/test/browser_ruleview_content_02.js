/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the rule-view content when the inspector gets opened via the page
// ctx-menu "inspect element"

const CONTENT = '<body style="color:red;">\
                   <div style="color:blue;">\
                     <p style="color:green;">\
                       <span style="color:yellow;">test element</span>\
                     </p>\
                   </div>\
                 </body>';

const STRINGS = Services.strings
  .createBundle("chrome://global/locale/devtools/styleinspector.properties");

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8," + CONTENT);

  info("Getting the test element");
  let element = getNode("span");

  info("Opening the inspector using the content context-menu");
  let onInspectorReady = gDevTools.once("inspector-ready");

  document.popupNode = element;
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let contextMenu = new nsContextMenu(contentAreaContextMenu);
  yield contextMenu.inspectNode();

  // Clean up context menu:
  contextMenu.hiding();

  yield onInspectorReady;

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = gDevTools.getToolbox(target);

  info("Getting the inspector and making sure it is fully updated");
  let inspector = toolbox.getPanel("inspector");
  yield inspector.once("inspector-updated");

  let view = inspector.sidebar.getWindowForTab("ruleview")["ruleview"].view;

  checkRuleViewContent(view);
});

function checkRuleViewContent({doc}) {
  info("Making sure the rule-view contains the expected content");

  let headers = [...doc.querySelectorAll(".ruleview-header")];
  is(headers.length, 3, "There are 3 headers for inherited rules");

  is(headers[0].textContent,
    STRINGS.formatStringFromName("rule.inheritedFrom", ["p"], 1),
    "The first header is correct");
  is(headers[1].textContent,
    STRINGS.formatStringFromName("rule.inheritedFrom", ["div"], 1),
    "The second header is correct");
  is(headers[2].textContent,
    STRINGS.formatStringFromName("rule.inheritedFrom", ["body"], 1),
    "The third header is correct");

  let rules = doc.querySelectorAll(".ruleview-rule");
  is(rules.length, 4, "There are 4 rules in the view");

  for (let rule of rules) {
    let selector = rule.querySelector(".ruleview-selector");
    is(selector.textContent,
      STRINGS.GetStringFromName("rule.sourceElement"),
      "The rule's selector is correct");

    let propertyNames = [...rule.querySelectorAll(".ruleview-propertyname")];
    is(propertyNames.length, 1, "There's only one property name, as expected");

    let propertyValues = [...rule.querySelectorAll(".ruleview-propertyvalue")];
    is(propertyValues.length, 1, "There's only one property value, as expected");
  }
}
