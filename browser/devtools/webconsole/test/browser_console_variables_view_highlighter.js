/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that variables view is linked to the inspector for highlighting and
// selecting DOM nodes

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-bug-952277-highlight-nodes-in-vview.html";

var gWebConsole, gJSTerm, gVariablesView, gToolbox;

function test() {
  loadTab(TEST_URI).then(() => {
    openConsole().then(hud => {
      consoleOpened(hud);
    });
  });
}

function consoleOpened(hud) {
  gWebConsole = hud;
  gJSTerm = hud.jsterm;
  gToolbox = gDevTools.getToolbox(hud.target);
  gJSTerm.execute("document.querySelectorAll('p')").then(onQSAexecuted);
}

function onQSAexecuted(msg) {
  ok(msg, "output message found");
  let anchor = msg.querySelector("a");
  ok(anchor, "object link found");

  gJSTerm.once("variablesview-fetched", onNodeListVviewFetched);

  executeSoon(() =>
    EventUtils.synthesizeMouse(anchor, 2, 2, {}, gWebConsole.iframeWindow)
  );
}

function onNodeListVviewFetched(aEvent, aVar) {
  gVariablesView = aVar._variablesView;
  ok(gVariablesView, "variables view object");

  // Transform the vview into an array we can filter properties from
  let props = [...aVar].map(([id, prop]) => [id, prop]);

  // These properties are the DOM nodes ones
  props = props.filter(v => v[0].match(/[0-9]+/));

  function hoverOverDomNodeVariableAndAssertHighlighter(index) {
    if (props[index]) {
      let prop = props[index][1];

      gToolbox.once("node-highlight", () => {
        ok(true, "The highlighter was shown on hover of the DOMNode");
        gToolbox.highlighterUtils.unhighlight().then(() => {
          clickOnDomNodeVariableAndAssertInspectorSelected(index);
        });
      });

      // Rather than trying to emulate a mouseenter event, let's call the
      // variable's highlightDomNode and see if it has the desired effect
      prop.highlightDomNode();
    } else {
      finishUp();
    }
  }

  function clickOnDomNodeVariableAndAssertInspectorSelected(index) {
    let prop = props[index][1];

    // Make sure the inspector is initialized so we can listen to its events
    gToolbox.initInspector().then(() => {
      // Rather than trying to click on the value here, let's just call the
      // variable's openNodeInInspector function and see if it has the
      // desired effect
      prop.openNodeInInspector().then(() => {
        is(gToolbox.currentToolId, "inspector",
           "The toolbox switched over the inspector on DOMNode click");
        gToolbox.selectTool("webconsole").then(() => {
          hoverOverDomNodeVariableAndAssertHighlighter(index + 1);
        });
      });
    });
  }

  hoverOverDomNodeVariableAndAssertHighlighter(0);
}

function finishUp() {
  gWebConsole = gJSTerm = gVariablesView = gToolbox = null;

  finishTest();
}
