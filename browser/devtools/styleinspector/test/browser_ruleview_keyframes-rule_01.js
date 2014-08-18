/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that keyframe rules and gutters are displayed correctly in the rule view

const TEST_URI = TEST_URL_ROOT + "doc_keyframeanimation.html";

let test = asyncTest(function*() {
  yield addTab(TEST_URI);

  let {toolbox, inspector, view} = yield openRuleView();

  yield testPacman(inspector, view);
  yield testBoxy(inspector, view);
  yield testMoxy(inspector, view);
});

function* testPacman(inspector, view) {
  info("Test content and gutter in the keyframes rule of #pacman");

  let {
    rules,
    element,
    elementStyle
  } = yield assertKeyframeRules("#pacman", inspector, view, {
    elementRulesNb: 2,
    keyframeRulesNb: 2,
    keyframesRules: ["pacman", "pacman"],
    keyframeRules: ["100%", "100%"]
  });

  let gutters = assertGutters(view, {
    guttersNbs: 2,
    gutterHeading: ["Keyframes pacman", "Keyframes pacman"]
  });
}

function* testBoxy(inspector, view) {
  info("Test content and gutter in the keyframes rule of #boxy");

  let {
    rules,
    element,
    elementStyle
  } = yield assertKeyframeRules("#boxy", inspector, view, {
    elementRulesNb: 3,
    keyframeRulesNb: 3,
    keyframesRules: ["boxy", "boxy", "boxy"],
    keyframeRules: ["10%", "20%", "100%"]
  });

  let gutters = assertGutters(view, {
    guttersNbs: 1,
    gutterHeading: ["Keyframes boxy"]
  });
}

function testMoxy(inspector, view) {
  info("Test content and gutter in the keyframes rule of #moxy");

  let {
    rules,
    element,
    elementStyle
  } = yield assertKeyframeRules("#moxy", inspector, view, {
    elementRulesNb: 3,
    keyframeRulesNb: 4,
    keyframesRules: ["boxy", "boxy", "boxy", "moxy"],
    keyframeRules: ["10%", "20%", "100%", "100%"]
  });

  let gutters = assertGutters(view, {
    guttersNbs: 2,
    gutterHeading: ["Keyframes boxy", "Keyframes moxy"]
  });
}

function* testNode(selector, inspector, view) {
  let element = getNode(selector);
  yield selectNode(selector, inspector);
  let elementStyle = view._elementStyle;
  return {element, elementStyle};
}

function* assertKeyframeRules(selector, inspector, view, expected) {
  let {element, elementStyle} = yield testNode(selector, inspector, view);

  let rules = {
    elementRules: elementStyle.rules.filter(rule => !rule.keyframes),
    keyframeRules: elementStyle.rules.filter(rule => rule.keyframes)
  };

  is(rules.elementRules.length, expected.elementRulesNb, selector +
    " has the correct number of non keyframe element rules");
  is(rules.keyframeRules.length, expected.keyframeRulesNb, selector +
    " has the correct number of keyframe rules");

  let i = 0;
  for (let keyframeRule of rules.keyframeRules) {
    ok(keyframeRule.keyframes.name == expected.keyframesRules[i],
      keyframeRule.keyframes.name + " has the correct keyframes name");
    ok(keyframeRule.domRule.keyText == expected.keyframeRules[i],
      keyframeRule.domRule.keyText + " selector heading is correct");
    i++;
  }

  return {rules, element, elementStyle};
}

function assertGutters(view, expected) {
  let gutters = view.element.querySelectorAll(".theme-gutter");

  is(gutters.length, expected.guttersNbs,
    "There are " + gutters.length + " gutter headings");

  let i = 0;
  for (let gutter of gutters) {
    is(gutter.textContent, expected.gutterHeading[i],
      "Correct " + gutter.textContent + " gutter headings");
    i++;
  }

  return gutters;
}
