/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that increasing/decreasing values in rule view using
// arrow keys works correctly.

let test = asyncTest(function*() {
  yield addTab("data:text/html,sample document for bug 722691");
  createDocument();
  let {toolbox, inspector, view} = yield openRuleView();

  yield selectNode("#test", inspector);

  yield testMarginIncrements(view);
  yield testVariousUnitIncrements(view);
  yield testHexIncrements(view);
  yield testRgbIncrements(view);
  yield testShorthandIncrements(view);
  yield testOddCases(view);
});

function createDocument() {
  content.document.body.innerHTML = '' +
    '<style>' +
    '  #test {' +
    '    margin-top:0px;' +
    '    padding-top: 0px;' +
    '    color:#000000;' +
    '    background-color: #000000;' +
    '  }' +
    '</style>' +
    '<div id="test"></div>';
}

function* testMarginIncrements(view) {
  info("Testing keyboard increments on the margin property");

  let idRuleEditor = getRuleViewRuleEditor(view, 1);
  let marginPropEditor = idRuleEditor.rule.textProps[0].editor;

  yield runIncrementTest(marginPropEditor, view, {
    1: {alt: true, start: "0px", end: "0.1px", selectAll: true},
    2: {start: "0px", end: "1px", selectAll: true},
    3: {shift: true, start: "0px", end: "10px", selectAll: true},
    4: {down: true, alt: true, start: "0.1px", end: "0px", selectAll: true},
    5: {down: true, start: "0px", end: "-1px", selectAll: true},
    6: {down: true, shift: true, start: "0px", end: "-10px", selectAll: true},
    7: {pageUp: true, shift: true, start: "0px", end: "100px", selectAll: true},
    8: {pageDown: true, shift: true, start: "0px", end: "-100px", selectAll: true}
  });
}

function* testVariousUnitIncrements(view) {
  info("Testing keyboard increments on values with various units");

  let idRuleEditor = getRuleViewRuleEditor(view, 1);
  let paddingPropEditor = idRuleEditor.rule.textProps[1].editor;

  yield runIncrementTest(paddingPropEditor, view, {
    1: {start: "0px", end: "1px", selectAll: true},
    2: {start: "0pt", end: "1pt", selectAll: true},
    3: {start: "0pc", end: "1pc", selectAll: true},
    4: {start: "0em", end: "1em", selectAll: true},
    5: {start: "0%",  end: "1%",  selectAll: true},
    6: {start: "0in", end: "1in", selectAll: true},
    7: {start: "0cm", end: "1cm", selectAll: true},
    8: {start: "0mm", end: "1mm", selectAll: true},
    9: {start: "0ex", end: "1ex", selectAll: true}
  });
};

function* testHexIncrements(view) {
  info("Testing keyboard increments with hex colors");

  let idRuleEditor = getRuleViewRuleEditor(view, 1);
  let hexColorPropEditor = idRuleEditor.rule.textProps[2].editor;

  yield runIncrementTest(hexColorPropEditor, view, {
    1: {start: "#CCCCCC", end: "#CDCDCD", selectAll: true},
    2: {shift: true, start: "#CCCCCC", end: "#DCDCDC", selectAll: true},
    3: {start: "#CCCCCC", end: "#CDCCCC", selection: [1,3]},
    4: {shift: true, start: "#CCCCCC", end: "#DCCCCC", selection: [1,3]},
    5: {start: "#FFFFFF", end: "#FFFFFF", selectAll: true},
    6: {down: true, shift: true, start: "#000000", end: "#000000", selectAll: true}
  });
};

function* testRgbIncrements(view) {
  info("Testing keyboard increments with rgb colors");

  let idRuleEditor = getRuleViewRuleEditor(view, 1);
  let rgbColorPropEditor = idRuleEditor.rule.textProps[3].editor;

  yield runIncrementTest(rgbColorPropEditor, view, {
    1: {start: "rgb(0,0,0)", end: "rgb(0,1,0)", selection: [6,7]},
    2: {shift: true, start: "rgb(0,0,0)", end: "rgb(0,10,0)", selection: [6,7]},
    3: {start: "rgb(0,255,0)", end: "rgb(0,255,0)", selection: [6,9]},
    4: {shift: true, start: "rgb(0,250,0)", end: "rgb(0,255,0)", selection: [6,9]},
    5: {down: true, start: "rgb(0,0,0)", end: "rgb(0,0,0)", selection: [6,7]},
    6: {down: true, shift: true, start: "rgb(0,5,0)", end: "rgb(0,0,0)", selection: [6,7]}
  });
};

function* testShorthandIncrements(view) {
  info("Testing keyboard increments within shorthand values");

  let idRuleEditor = getRuleViewRuleEditor(view, 1);
  let paddingPropEditor = idRuleEditor.rule.textProps[1].editor;

  yield runIncrementTest(paddingPropEditor, view, {
    1: {start: "0px 0px 0px 0px", end: "0px 1px 0px 0px", selection: [4,7]},
    2: {shift: true, start: "0px 0px 0px 0px", end: "0px 10px 0px 0px", selection: [4,7]},
    3: {start: "0px 0px 0px 0px", end: "1px 0px 0px 0px", selectAll: true},
    4: {shift: true, start: "0px 0px 0px 0px", end: "10px 0px 0px 0px", selectAll: true},
    5: {down: true, start: "0px 0px 0px 0px", end: "0px 0px -1px 0px", selection: [8,11]},
    6: {down: true, shift: true, start: "0px 0px 0px 0px", end: "-10px 0px 0px 0px", selectAll: true}
  });
};

function* testOddCases(view) {
  info("Testing some more odd cases");

  let idRuleEditor = getRuleViewRuleEditor(view, 1);
  let marginPropEditor = idRuleEditor.rule.textProps[0].editor;

  yield runIncrementTest(marginPropEditor, view, {
    1: {start: "98.7%", end: "99.7%", selection: [3,3]},
    2: {alt: true, start: "98.7%", end: "98.8%", selection: [3,3]},
    3: {start: "0", end: "1"},
    4: {down: true, start: "0", end: "-1"},
    5: {start: "'a=-1'", end: "'a=0'", selection: [4,4]},
    6: {start: "0 -1px", end: "0 0px", selection: [2,2]},
    7: {start: "url(-1)", end: "url(-1)", selection: [4,4]},
    8: {start: "url('test1.1.png')", end: "url('test1.2.png')", selection: [11,11]},
    9: {start: "url('test1.png')", end: "url('test2.png')", selection: [9,9]},
    10: {shift: true, start: "url('test1.1.png')", end: "url('test11.1.png')", selection: [9,9]},
    11: {down: true, start: "url('test-1.png')", end: "url('test-2.png')", selection: [9,11]},
    12: {start: "url('test1.1.png')", end: "url('test1.2.png')", selection: [11,12]},
    13: {down: true, alt: true, start: "url('test-0.png')", end: "url('test--0.1.png')", selection: [10,11]},
    14: {alt: true, start: "url('test--0.1.png')", end: "url('test-0.png')", selection: [10,14]}
  });
};

function* runIncrementTest(propertyEditor, view, tests) {
  let editor = yield focusEditableField(propertyEditor.valueSpan);

  for(let test in tests) {
    yield testIncrement(editor, tests[test], view, propertyEditor);
  }
}

function* testIncrement(editor, options, view, {ruleEditor}) {
  editor.input.value = options.start;
  let input = editor.input;

  if (options.selectAll) {
    input.select();
  } else if (options.selection) {
    input.setSelectionRange(options.selection[0], options.selection[1]);
  }

  is(input.value, options.start, "Value initialized at " + options.start);

  let onModifications = ruleEditor.rule._applyingModifications;
  let onKeyUp = once(input, "keyup");
  let key;
  key = options.down ? "VK_DOWN" : "VK_UP";
  key = options.pageDown ? "VK_PAGE_DOWN" : options.pageUp ? "VK_PAGE_UP" : key;
  EventUtils.synthesizeKey(key, {altKey: options.alt, shiftKey: options.shift},
    view.doc.defaultView);
  yield onKeyUp;
  yield onModifications;

  is(editor.input.value, options.end, "Value changed to " + options.end);
}
