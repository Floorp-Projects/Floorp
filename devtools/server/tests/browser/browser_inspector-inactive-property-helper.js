/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test expected outputs of the inactivePropertyHelper's isPropertyUsed function.

// This is more of a unit test than a mochitest-browser test, but can't be tested with an
// xpcshell test as the inactivePropertyHelper requires the DOM to work (e.g for computed
// styles).

add_task(async function() {
  await pushPref("devtools.inspector.inactive.css.enabled", true);

  const {inactivePropertyHelper} = require("devtools/server/actors/utils/inactive-property-helper");
  let { isPropertyUsed } = inactivePropertyHelper;
  isPropertyUsed = isPropertyUsed.bind(inactivePropertyHelper);

  // A single test case is an object of the following shape:
  // - {String} info: a summary of the test case
  // - {String} property: the CSS property that should be tested
  // - {String} tagName: the tagName of the element we're going to test
  // - {Array<String>} rules: An array of the rules that will be applied on the element.
  //                          This can't be empty as isPropertyUsed need a rule.
  // - {Integer} ruleIndex (optional): If there are multiples rules in `rules`, the index
  //                                   of the one that should be tested in isPropertyUsed.
  // - {Boolean} isActive: should the property be active (isPropertyUsed `used` result).
  const tests = [{
    info: "vertical-align is inactive on a block element",
    property: "vertical-align",
    tagName: "div",
    rules: ["div { vertical-align: top; }"],
    isActive: false,
  }, {
    info: "vertical-align is inactive on a span with display block",
    property: "vertical-align",
    tagName: "span",
    rules: ["span { vertical-align: top; display: block;}"],
    isActive: false,
  }, {
    info: "vertical-align is active on a div with display inline-block",
    property: "vertical-align",
    tagName: "div",
    rules: ["div { vertical-align: top; display: inline-block;}"],
    isActive: true,
  }, {
    info: "vertical-align is active on a table-cell",
    property: "vertical-align",
    tagName: "div",
    rules: ["div { vertical-align: top; display: table-cell;}"],
    isActive: true,
  }, {
    info: "vertical-align is active on a block element ::first-letter",
    property: "vertical-align",
    tagName: "div",
    rules: ["div::first-letter { vertical-align: top; }"],
    isActive: true,
  }, {
    info: "vertical-align is active on a block element ::first-line",
    property: "vertical-align",
    tagName: "div",
    rules: ["div::first-line { vertical-align: top; }"],
    isActive: true,
  }, {
    info: "vertical-align is active on an inline element",
    property: "vertical-align",
    tagName: "span",
    rules: ["span { vertical-align: top; }"],
    isActive: true,
  }];

  for (const {info: summary, property, tagName, rules, ruleIndex, isActive} of tests) {
    info(summary);

    // Create an element which will contain the test elements.
    const main = document.createElement("main");
    document.firstElementChild.appendChild(main);

    // Create the element and insert the rules
    const el = document.createElement(tagName);
    const style = document.createElement("style");
    main.append(style, el);

    for (const dataRule of rules) {
      style.sheet.insertRule(dataRule);
    }
    const rule = style.sheet.cssRules[ruleIndex || 0 ];

    const {used} = isPropertyUsed(el, getComputedStyle(el), rule, property);
    ok(used === isActive, `"${property}" is ${isActive ? "active" : "inactive"}`);

    main.remove();
  }
});
