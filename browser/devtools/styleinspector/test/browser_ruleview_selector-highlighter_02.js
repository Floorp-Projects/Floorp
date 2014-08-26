/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter is shown when hovering over a selector
// in the rule-view

// Note that in this test, we mock the highlighter front, merely testing the
// behavior of the style-inspector UI for now

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    background: red;',
  '  }',
  '  p {',
  '    color: white;',
  '  }',
  '</style>',
  '<p>Testing the selector highlighter</p>'
].join("\n");

const TYPE = "SelectorHighlighter";

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8," + PAGE_CONTENT);

  let {inspector, view: rView} = yield openRuleView();

  // Mock the highlighter front to get the reference of the NodeFront
  let HighlighterFront = {
    isShown: false,
    nodeFront: null,
    options: null,
    show: function(nodeFront, options) {
      this.nodeFront = nodeFront;
      this.options = options;
      this.isShown = true;
    },
    hide: function() {
      this.nodeFront = null;
      this.options = null;
      this.isShown = false;
    }
  };

  // Inject the mock highlighter in the rule-view
  rView.highlighters.promises[TYPE] = {
    then: function(cb) {
      cb(HighlighterFront);
    }
  };

  let selectorSpan = getRuleViewSelector(rView, "body").firstElementChild;

  info("Checking that the HighlighterFront's show/hide methods are called");
  rView.highlighters._onMouseMove({target: selectorSpan});
  ok(HighlighterFront.isShown, "The highlighter is shown");
  rView.highlighters._onMouseLeave();
  ok(!HighlighterFront.isShown, "The highlighter is hidden");

  info("Checking that the right NodeFront reference and options are passed");
  yield selectNode("p", inspector);
  let selectorSpan = getRuleViewSelector(rView, "p").firstElementChild;
  rView.highlighters._onMouseMove({target: selectorSpan});
  is(HighlighterFront.nodeFront.tagName, "P",
    "The right NodeFront is passed to the highlighter (1)");
  is(HighlighterFront.options.selector, "p",
    "The right selector option is passed to the highlighter (1)");

  yield selectNode("body", inspector);
  let selectorSpan = getRuleViewSelector(rView, "body").firstElementChild;
  rView.highlighters._onMouseMove({target: selectorSpan});
  is(HighlighterFront.nodeFront.tagName, "BODY",
    "The right NodeFront is passed to the highlighter (2)");
  is(HighlighterFront.options.selector, "body",
    "The right selector option is passed to the highlighter (2)");

  info("Checking that the highlighter gets hidden when hovering somewhere else");
  let {valueSpan} = getRuleViewProperty(rView, "body", "background");
  rView.highlighters._onMouseMove({target: valueSpan});
  ok(!HighlighterFront.isShown, "The highlighter is hidden");
});
