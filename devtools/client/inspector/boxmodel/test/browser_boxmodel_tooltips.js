/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the regions in the box model view have tooltips, and that individual
// values too. Also test that values that are set from a css rule have tooltips
// referencing the rule.

const TEST_URI = "<style>" +
  "#div1 { color: red; margin: 3em; }\n" +
  "#div2 { border-bottom: 1px solid black; background: red; }\n" +
  "html, body, #div3 { box-sizing: border-box; padding: 0 2em; }" +
  "</style>" +
  "<div id='div1'></div><div id='div2'></div><div id='div3'></div>";

// Test data for the tooltips over individual values.
// Each entry should contain:
// - selector: The selector for the node to be selected before starting to test
// - values: An array containing objects for each of the values that are defined
//   by css rules. Each entry should contain:
//   - name: the name of the property that is set by the css rule
//   - ruleSelector: the selector of the rule
//   - styleSheetLocation: the fileName:lineNumber
const VALUES_TEST_DATA = [{
  selector: "#div1",
  values: [{
    name: "margin-top",
    ruleSelector: "#div1",
    styleSheetLocation: "inline:1"
  }, {
    name: "margin-right",
    ruleSelector: "#div1",
    styleSheetLocation: "inline:1"
  }, {
    name: "margin-bottom",
    ruleSelector: "#div1",
    styleSheetLocation: "inline:1"
  }, {
    name: "margin-left",
    ruleSelector: "#div1",
    styleSheetLocation: "inline:1"
  }]
}, {
  selector: "#div2",
  values: [{
    name: "border-bottom-width",
    ruleSelector: "#div2",
    styleSheetLocation: "inline:2"
  }]
}, {
  selector: "#div3",
  values: [{
    name: "padding-top",
    ruleSelector: "html, body, #div3",
    styleSheetLocation: "inline:3"
  }, {
    name: "padding-right",
    ruleSelector: "html, body, #div3",
    styleSheetLocation: "inline:3"
  }, {
    name: "padding-bottom",
    ruleSelector: "html, body, #div3",
    styleSheetLocation: "inline:3"
  }, {
    name: "padding-left",
    ruleSelector: "html, body, #div3",
    styleSheetLocation: "inline:3"
  }]
}];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, boxmodel} = await openLayoutView();

  info("Checking the regions tooltips");

  ok(boxmodel.document.querySelector(".boxmodel-margins").hasAttribute("title"),
    "The margin region has a tooltip");
  is(boxmodel.document.querySelector(".boxmodel-margins").getAttribute("title"), "margin",
    "The margin region has the correct tooltip content");

  ok(boxmodel.document.querySelector(".boxmodel-borders").hasAttribute("title"),
    "The border region has a tooltip");
  is(boxmodel.document.querySelector(".boxmodel-borders").getAttribute("title"), "border",
    "The border region has the correct tooltip content");

  ok(boxmodel.document.querySelector(".boxmodel-paddings").hasAttribute("title"),
    "The padding region has a tooltip");
  is(boxmodel.document.querySelector(".boxmodel-paddings").getAttribute("title"),
    "padding", "The padding region has the correct tooltip content");

  ok(boxmodel.document.querySelector(".boxmodel-content").hasAttribute("title"),
    "The content region has a tooltip");
  is(boxmodel.document.querySelector(".boxmodel-content").getAttribute("title"),
    "content", "The content region has the correct tooltip content");

  for (const {selector, values} of VALUES_TEST_DATA) {
    info("Selecting " + selector + " and checking the values tooltips");
    await selectNode(selector, inspector);

    info("Iterate over all values");
    for (const key in boxmodel.map) {
      if (key === "position") {
        continue;
      }

      const name = boxmodel.map[key].property;
      const expectedTooltipData = values.find(o => o.name === name);
      const el = boxmodel.document.querySelector(boxmodel.map[key].selector);

      ok(el.hasAttribute("title"), "The " + name + " value has a tooltip");

      if (expectedTooltipData) {
        info("The " + name + " value comes from a css rule");
        const expectedTooltip = name + "\n" + expectedTooltipData.ruleSelector +
                              "\n" + expectedTooltipData.styleSheetLocation;
        is(el.getAttribute("title"), expectedTooltip, "The tooltip is correct");
      } else {
        info("The " + name + " isn't set by a css rule");
        is(el.getAttribute("title"), name, "The tooltip is correct");
      }
    }
  }
});
