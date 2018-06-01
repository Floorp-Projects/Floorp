/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that user agent styles are never editable via
// the UI

const TEST_URI = `
  <blockquote type=cite>
   <pre _moz_quote=true>
     inspect <a href='foo' style='color:orange'>user agent</a> styles
   </pre>
  </blockquote>
`;

var PREF_UA_STYLES = "devtools.inspector.showUserAgentStyles";

add_task(async function() {
  info("Starting the test with the pref set to true before toolbox is opened");
  Services.prefs.setBoolPref(PREF_UA_STYLES, true);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  await userAgentStylesUneditable(inspector, view);

  info("Resetting " + PREF_UA_STYLES);
  Services.prefs.clearUserPref(PREF_UA_STYLES);
});

async function userAgentStylesUneditable(inspector, view) {
  info("Making sure that UI is not editable for user agent styles");

  await selectNode("a", inspector);
  const uaRules = view._elementStyle.rules.filter(rule=>!rule.editor.isEditable);

  for (const rule of uaRules) {
    ok(rule.editor.element.hasAttribute("uneditable"),
      "UA rules have uneditable attribute");

    const firstProp = rule.textProps.filter(p => !p.invisible)[0];

    ok(!firstProp.editor.nameSpan._editable,
      "nameSpan is not editable");
    ok(!firstProp.editor.valueSpan._editable,
      "valueSpan is not editable");
    ok(!rule.editor.closeBrace._editable, "closeBrace is not editable");

    const colorswatch = rule.editor.element
      .querySelector(".ruleview-colorswatch");
    if (colorswatch) {
      ok(!view.tooltips.getTooltip("colorPicker").swatches.has(colorswatch),
        "The swatch is not editable");
    }
  }
}
