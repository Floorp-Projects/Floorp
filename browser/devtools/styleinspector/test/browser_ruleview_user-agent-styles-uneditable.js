/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that user agent styles are never editable via
// the UI

let PREF_UA_STYLES = "devtools.inspector.showUserAgentStyles";
const TEST_URI = "data:text/html;charset=utf-8," +
  "<blockquote type=cite>" +
  " <pre _moz_quote=true>" +
  "   inspect <a href='foo' style='color:orange'>user agent</a> styles" +
  " </pre>" +
  "</blockquote>";

let test = asyncTest(function*() {
  info ("Starting the test with the pref set to true before toolbox is opened");
  Services.prefs.setBoolPref(PREF_UA_STYLES, true);

  info ("Opening the testcase and toolbox")
  yield addTab(TEST_URI);
  let {toolbox, inspector, view} = yield openRuleView();

  yield userAgentStylesUneditable(inspector, view);

  info("Resetting " + PREF_UA_STYLES);
  Services.prefs.clearUserPref(PREF_UA_STYLES);
});

function* userAgentStylesUneditable(inspector, view) {
  info ("Making sure that UI is not editable for user agent styles");

  yield selectNode("a", inspector);
  let uaRules = view._elementStyle.rules.filter(rule=>!rule.editor.isEditable);

  for (let rule of uaRules) {
    ok (rule.editor.element.hasAttribute("uneditable"), "UA rules have uneditable attribute");

    ok (!rule.textProps[0].editor.nameSpan._editable, "nameSpan is not editable");
    ok (!rule.textProps[0].editor.valueSpan._editable, "valueSpan is not editable");
    ok (!rule.editor.closeBrace._editable, "closeBrace is not editable");

    let colorswatch = rule.editor.element.querySelector(".ruleview-colorswatch");
    if (colorswatch) {
      ok (!view.tooltips.colorPicker.swatches.has(colorswatch), "The swatch is not editable");
    }
  }
}
