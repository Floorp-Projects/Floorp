/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* globals getTestActorWithoutToolbox */
"use strict";

// Test the rule-view content when the inspector gets opened via the page
// ctx-menu "inspect element"

const CONTENT = `
  <body style="color:red;">
    <div style="color:blue;">
      <p style="color:green;">
        <span style="color:yellow;">test element</span>
      </p>
    </div>
  </body>
`;

const STRINGS = Services.strings
  .createBundle("chrome://devtools-shared/locale/styleinspector.properties");

add_task(function*() {
  let tab = yield addTab("data:text/html;charset=utf-8," + CONTENT);

  let testActor = yield getTestActorWithoutToolbox(tab);
  let inspector = yield clickOnInspectMenuItem(testActor, "span");

  checkRuleViewContent(inspector.ruleview.view);
});

function checkRuleViewContent({styleDocument}) {
  info("Making sure the rule-view contains the expected content");

  let headers = [...styleDocument.querySelectorAll(".ruleview-header")];
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

  let rules = styleDocument.querySelectorAll(".ruleview-rule");
  is(rules.length, 4, "There are 4 rules in the view");

  for (let rule of rules) {
    let selector = rule.querySelector(".ruleview-selectorcontainer");
    is(selector.textContent,
      STRINGS.GetStringFromName("rule.sourceElement"),
      "The rule's selector is correct");

    let propertyNames = [...rule.querySelectorAll(".ruleview-propertyname")];
    is(propertyNames.length, 1,
       "There's only one property name, as expected");

    let propertyValues = [...rule.querySelectorAll(".ruleview-propertyvalue")];
    is(propertyValues.length, 1,
       "There's only one property value, as expected");
  }
}

