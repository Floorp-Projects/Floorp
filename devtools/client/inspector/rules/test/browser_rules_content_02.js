/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + CONTENT);

  // const commands = await CommandsFactory.forTab(tab);
  // // Initialize the TargetCommands which require some async stuff to be done
  // // before being fully ready. This will define the `targetCommand.targetFront` attribute.
  // await commands.targetCommand.startListening();
  const inspector = await clickOnInspectMenuItem("span");

  checkRuleViewContent(inspector.getPanel("ruleview").view);
});

function checkRuleViewContent({ styleDocument }) {
  info("Making sure the rule-view contains the expected content");

  const headers = [...styleDocument.querySelectorAll(".ruleview-header")];
  is(headers.length, 3, "There are 3 headers for inherited rules");

  is(
    headers[0].textContent,
    STYLE_INSPECTOR_L10N.getFormatStr("rule.inheritedFrom", "p"),
    "The first header is correct"
  );
  is(
    headers[1].textContent,
    STYLE_INSPECTOR_L10N.getFormatStr("rule.inheritedFrom", "div"),
    "The second header is correct"
  );
  is(
    headers[2].textContent,
    STYLE_INSPECTOR_L10N.getFormatStr("rule.inheritedFrom", "body"),
    "The third header is correct"
  );

  const rules = styleDocument.querySelectorAll(".ruleview-rule");
  is(rules.length, 4, "There are 4 rules in the view");

  for (const rule of rules) {
    const selector = rule.querySelector(".ruleview-selectorcontainer");
    is(
      selector.textContent,
      STYLE_INSPECTOR_L10N.getStr("rule.sourceElement"),
      "The rule's selector is correct"
    );

    const propertyNames = [...rule.querySelectorAll(".ruleview-propertyname")];
    is(propertyNames.length, 1, "There's only one property name, as expected");

    const propertyValues = [
      ...rule.querySelectorAll(".ruleview-propertyvalue"),
    ];
    is(
      propertyValues.length,
      1,
      "There's only one property value, as expected"
    );
  }
}
