/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the regions in the layout-view have tooltips, and that individual
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
    styleSheetLocation: "null:1"
  }, {
    name: "margin-right",
    ruleSelector: "#div1",
    styleSheetLocation: "null:1"
  }, {
    name: "margin-bottom",
    ruleSelector: "#div1",
    styleSheetLocation: "null:1"
  }, {
    name: "margin-left",
    ruleSelector: "#div1",
    styleSheetLocation: "null:1"
  }]
},{
  selector: "#div2",
  values: [{
    name: "border-bottom-width",
    ruleSelector: "#div2",
    styleSheetLocation: "null:2"
  }]
},{
  selector: "#div3",
  values: [{
    name: "padding-top",
    ruleSelector: "html, body, #div3",
    styleSheetLocation: "null:3"
  }, {
    name: "padding-right",
    ruleSelector: "html, body, #div3",
    styleSheetLocation: "null:3"
  }, {
    name: "padding-bottom",
    ruleSelector: "html, body, #div3",
    styleSheetLocation: "null:3"
  }, {
    name: "padding-left",
    ruleSelector: "html, body, #div3",
    styleSheetLocation: "null:3"
  }]
}];

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openLayoutView();

  info("Checking the regions tooltips");

  ok(view.doc.querySelector("#margins").hasAttribute("title"),
    "The margin region has a tooltip");
  is(view.doc.querySelector("#margins").getAttribute("title"), "margin",
    "The margin region has the correct tooltip content");

  ok(view.doc.querySelector("#borders").hasAttribute("title"),
    "The border region has a tooltip");
  is(view.doc.querySelector("#borders").getAttribute("title"), "border",
    "The border region has the correct tooltip content");

  ok(view.doc.querySelector("#padding").hasAttribute("title"),
    "The padding region has a tooltip");
  is(view.doc.querySelector("#padding").getAttribute("title"), "padding",
    "The padding region has the correct tooltip content");

  ok(view.doc.querySelector("#content").hasAttribute("title"),
    "The content region has a tooltip");
  is(view.doc.querySelector("#content").getAttribute("title"), "content",
    "The content region has the correct tooltip content");

  for (let {selector, values} of VALUES_TEST_DATA) {
    info("Selecting " + selector + " and checking the values tooltips");
    yield selectNode(selector, inspector);

    info("Iterate over all values");
    for (let key in view.map) {
      if (key === "position") {
        continue;
      }

      let name = view.map[key].property;
      let expectedTooltipData = values.find(o => o.name === name);
      let el = view.doc.querySelector(view.map[key].selector);

      ok(el.hasAttribute("title"), "The " + name + " value has a tooltip");

      if (expectedTooltipData) {
        info("The " + name + " value comes from a css rule");
        let expectedTooltip = name + "\n" + expectedTooltipData.ruleSelector +
                              "\n" + expectedTooltipData.styleSheetLocation;
        is(el.getAttribute("title"), expectedTooltip, "The tooltip is correct");
      } else {
        info("The " + name + " isn't set by a css rule");
        is(el.getAttribute("title"), name, "The tooltip is correct");
      }
    }
  }
});
