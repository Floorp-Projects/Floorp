/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that increasing/decreasing values in rule view using
// arrow keys works correctly.

let tempScope = {};
Cu.import("resource:///modules/devtools/CssRuleView.jsm", tempScope);
let CssRuleView = tempScope.CssRuleView;
let _ElementStyle = tempScope._ElementStyle;

let doc;
let ruleDialog;
let ruleView;

function setUpTests()
{
  doc.body.innerHTML = '<div id="test" style="' +
                           'margin-top:0px;' +
                           'padding-top: 0px;' +
                           'color:#000000;' +
                           'background-color: #000000; >"'+
                       '</div>';
  let testElement = doc.getElementById("test");
  ruleDialog = openDialog("chrome://browser/content/devtools/cssruleview.xhtml",
                          "cssruleviewtest",
                          "width=350,height=350");
  ruleDialog.addEventListener("load", function onLoad(evt) {
    ruleDialog.removeEventListener("load", onLoad, true);
    let doc = ruleDialog.document;
    ruleView = new CssRuleView(doc);
    doc.documentElement.appendChild(ruleView.element);
    ruleView.highlight(testElement);
    waitForFocus(runTests, ruleDialog);
  }, true);
}

function runTests()
{
  let idRuleEditor = ruleView.element.children[0]._ruleEditor;
  let marginPropEditor = idRuleEditor.rule.textProps[0].editor;
  let paddingPropEditor = idRuleEditor.rule.textProps[1].editor;
  let hexColorPropEditor = idRuleEditor.rule.textProps[2].editor;
  let rgbColorPropEditor = idRuleEditor.rule.textProps[3].editor;

  (function() {
    info("INCREMENTS");
    newTest( marginPropEditor, {
      1: { alt: true, start: "0px", end: "0.1px", selectAll: true },
      2: { start: "0px", end: "1px", selectAll: true },
      3: { shift: true, start: "0px", end: "10px", selectAll: true },
      4: { down: true, alt: true, start: "0.1px", end: "0px", selectAll: true },
      5: { down: true, start: "0px", end: "-1px", selectAll: true },
      6: { down: true, shift: true, start: "0px", end: "-10px", selectAll: true },
      7: { pageUp: true, shift: true, start: "0px", end: "100px", selectAll: true },
      8: { pageDown: true, shift: true, start: "0px", end: "-100px", selectAll: true,
           nextTest: test2 }
    });
    EventUtils.synthesizeMouse(marginPropEditor.valueSpan, 1, 1, {}, ruleDialog);
  })();

  function test2() {
    info("UNITS");
    newTest( paddingPropEditor, {
      1: { start: "0px", end: "1px", selectAll: true },
      2: { start: "0pt", end: "1pt", selectAll: true },
      3: { start: "0pc", end: "1pc", selectAll: true },
      4: { start: "0em", end: "1em", selectAll: true },
      5: { start: "0%",  end: "1%",  selectAll: true },
      6: { start: "0in", end: "1in", selectAll: true },
      7: { start: "0cm", end: "1cm", selectAll: true },
      8: { start: "0mm", end: "1mm", selectAll: true },
      9: { start: "0ex", end: "1ex", selectAll: true,
           nextTest: test3 }
    });
    EventUtils.synthesizeMouse(paddingPropEditor.valueSpan, 1, 1, {}, ruleDialog);
  };

  function test3() {
    info("HEX COLORS");
    newTest( hexColorPropEditor, {
      1: { start: "#CCCCCC", end: "#CDCDCD", selectAll: true},
      2: { shift: true, start: "#CCCCCC", end: "#DCDCDC", selectAll: true },
      3: { start: "#CCCCCC", end: "#CDCCCC", selection: [1,3] },
      4: { shift: true, start: "#CCCCCC", end: "#DCCCCC", selection: [1,3] },
      5: { start: "#FFFFFF", end: "#FFFFFF", selectAll: true },
      6: { down: true, shift: true, start: "#000000", end: "#000000", selectAll: true,
           nextTest: test4 }
    });
    EventUtils.synthesizeMouse(hexColorPropEditor.valueSpan, 1, 1, {}, ruleDialog);
  };

  function test4() {
    info("RGB COLORS");
    newTest( rgbColorPropEditor, {
      1: { start: "rgb(0,0,0)", end: "rgb(0,1,0)", selection: [6,7] },
      2: { shift: true, start: "rgb(0,0,0)", end: "rgb(0,10,0)", selection: [6,7] },
      3: { start: "rgb(0,255,0)", end: "rgb(0,255,0)", selection: [6,9] },
      4: { shift: true, start: "rgb(0,250,0)", end: "rgb(0,255,0)", selection: [6,9] },
      5: { down: true, start: "rgb(0,0,0)", end: "rgb(0,0,0)", selection: [6,7] },
      6: { down: true, shift: true, start: "rgb(0,5,0)", end: "rgb(0,0,0)", selection: [6,7],
           nextTest: test5 }
    });
    EventUtils.synthesizeMouse(rgbColorPropEditor.valueSpan, 1, 1, {}, ruleDialog);
  };

  function test5() {
    info("SHORTHAND");
    newTest( paddingPropEditor, {
      1: { start: "0px 0px 0px 0px", end: "0px 1px 0px 0px", selection: [4,7] },
      2: { shift: true, start: "0px 0px 0px 0px", end: "0px 10px 0px 0px", selection: [4,7] },
      3: { start: "0px 0px 0px 0px", end: "1px 0px 0px 0px", selectAll: true },
      4: { shift: true, start: "0px 0px 0px 0px", end: "10px 0px 0px 0px", selectAll: true },
      5: { down: true, start: "0px 0px 0px 0px", end: "0px 0px -1px 0px", selection: [8,11] },
      6: { down: true, shift: true, start: "0px 0px 0px 0px", end: "-10px 0px 0px 0px", selectAll: true,
           nextTest: test6 }
    });
    EventUtils.synthesizeMouse(paddingPropEditor.valueSpan, 1, 1, {}, ruleDialog);
  };

  function test6() {
    info("ODD CASES");
    newTest( marginPropEditor, {
      1: { start: "98.7%", end: "99.7%", selection: [3,3] },
      2: { alt: true, start: "98.7%", end: "98.8%", selection: [3,3] },
      3: { start: "0", end: "1" },
      4: { down: true, start: "0", end: "-1" },
      5: { start: "'a=-1'", end: "'a=0'", selection: [4,4] },
      6: { start: "0 -1px", end: "0 0px", selection: [2,2] },
      7: { start: "url(-1)", end: "url(-1)", selection: [4,4] },
      8: { start: "url('test1.1.png')", end: "url('test1.2.png')", selection: [11,11] },
      9: { start: "url('test1.png')", end: "url('test2.png')", selection: [9,9] },
      10: { shift: true, start: "url('test1.1.png')", end: "url('test11.1.png')", selection: [9,9] },
      11: { down: true, start: "url('test-1.png')", end: "url('test-2.png')", selection: [9,11] },
      12: { start: "url('test1.1.png')", end: "url('test1.2.png')", selection: [11,12] },
      13: { down: true, alt: true, start: "url('test-0.png')", end: "url('test--0.1.png')", selection: [10,11] },
      14: { alt: true, start: "url('test--0.1.png')", end: "url('test-0.png')", selection: [10,14],
           endTest: true }
    });
    EventUtils.synthesizeMouse(marginPropEditor.valueSpan, 1, 1, {}, ruleDialog);
  };
}

function newTest( propEditor, tests )
{
  waitForEditorFocus(propEditor.element, function onElementFocus(aEditor) {
    for( test in tests) {
      testIncrement( aEditor, tests[test] );
    }
  }, false);
}

function testIncrement( aEditor, aOptions )
{
  aEditor.input.value = aOptions.start;
  let input = aEditor.input;
  if ( aOptions.selectAll ) {
    input.select();
  } else if ( aOptions.selection ) {
    input.setSelectionRange(aOptions.selection[0], aOptions.selection[1]);
  }
  is(input.value, aOptions.start, "Value initialized at " + aOptions.start);
  input.addEventListener("keyup", function onIncrementUp() {
    input.removeEventListener("keyup", onIncrementUp, false);
    input = aEditor.input;
    is(input.value, aOptions.end, "Value changed to " + aOptions.end);
    if( aOptions.nextTest) {
      aOptions.nextTest();
    }
    else if( aOptions.endTest ) {
      finishTest();
    }
  }, false);
  let key;
  key = ( aOptions.down ) ? "VK_DOWN" : "VK_UP";
  key = ( aOptions.pageDown ) ? "VK_PAGE_DOWN" : ( aOptions.pageUp ) ? "VK_PAGE_UP" : key;
  EventUtils.synthesizeKey(key,
                          {altKey: aOptions.alt, shiftKey: aOptions.shift},
                          ruleDialog);
}

function finishTest()
{
  ruleView.clear();
  ruleDialog.close();
  ruleDialog = ruleView = null;
  doc = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onload, true);
    doc = content.document;
    waitForFocus(setUpTests, content);
  }, true);
  content.location = "data:text/html,sample document for bug 722691";
}
