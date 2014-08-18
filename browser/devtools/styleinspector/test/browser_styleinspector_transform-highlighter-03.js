/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the css transform highlighter is shown when hovering over transform
// properties

// Note that in this test, we mock the highlighter front, merely testing the
// behavior of the style-inspector UI for now

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  html {',
  '    transform: scale(.9);',
  '  }',
  '  body {',
  '    transform: skew(16deg);',
  '    color: purple;',
  '  }',
  '</style>',
  'Test the css transform highlighter'
].join("\n");

const TYPE = "CssTransformHighlighter";

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8," + PAGE_CONTENT);

  let {inspector, view: rView} = yield openRuleView();

  // Mock the highlighter front to get the reference of the NodeFront
  let HighlighterFront = {
    isShown: false,
    nodeFront: null,
    nbOfTimesShown: 0,
    show: function(nodeFront) {
      this.nodeFront = nodeFront;
      this.isShown = true;
      this.nbOfTimesShown ++;
    },
    hide: function() {
      this.nodeFront = null;
      this.isShown = false;
    }
  };

  // Inject the mock highlighter in the rule-view
  rView.highlighters.promises[TYPE] = {
    then: function(cb) {
      cb(HighlighterFront);
    }
  };

  let {valueSpan} = getRuleViewProperty(rView, "body", "transform");

  info("Checking that the HighlighterFront's show/hide methods are called");
  rView.highlighters._onMouseMove({target: valueSpan});
  ok(HighlighterFront.isShown, "The highlighter is shown");
  rView.highlighters._onMouseLeave();
  ok(!HighlighterFront.isShown, "The highlighter is hidden");

  info("Checking that hovering several times over the same property doesn't" +
    " show the highlighter several times");
  let nb = HighlighterFront.nbOfTimesShown;
  rView.highlighters._onMouseMove({target: valueSpan});
  is(HighlighterFront.nbOfTimesShown, nb + 1, "The highlighter was shown once");
  rView.highlighters._onMouseMove({target: valueSpan});
  rView.highlighters._onMouseMove({target: valueSpan});
  is(HighlighterFront.nbOfTimesShown, nb + 1,
    "The highlighter was shown once, after several mousemove");

  info("Checking that the right NodeFront reference is passed");
  yield selectNode("html", inspector);
  let {valueSpan} = getRuleViewProperty(rView, "html", "transform");
  rView.highlighters._onMouseMove({target: valueSpan});
  is(HighlighterFront.nodeFront.tagName, "HTML",
    "The right NodeFront is passed to the highlighter (1)");

  yield selectNode("body", inspector);
  let {valueSpan} = getRuleViewProperty(rView, "body", "transform");
  rView.highlighters._onMouseMove({target: valueSpan});
  is(HighlighterFront.nodeFront.tagName, "BODY",
    "The right NodeFront is passed to the highlighter (2)");

  info("Checking that the highlighter gets hidden when hovering a non-transform property");
  let {valueSpan} = getRuleViewProperty(rView, "body", "color");
  rView.highlighters._onMouseMove({target: valueSpan});
  ok(!HighlighterFront.isShown, "The highlighter is hidden");
});
