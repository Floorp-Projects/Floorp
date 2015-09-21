/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the inspector links in the webconsole output for DOM Nodes actually
// open the inspector and select the right node

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-console-output-dom-elements.html";

const TEST_DATA = [
  {
    // The first test shouldn't be returning the body element as this is the
    // default selected node, so re-selecting it won't fire the
    // inspector-updated event
    input: "testNode()",
    output: '<p some-attribute="some-value">',
    tagName: "P",
    attrs: [{name: "some-attribute", value: "some-value"}]
  },
  {
    input: "testBodyNode()",
    output: '<body id="body-id" class="body-class">',
    tagName: "BODY",
    attrs: [
      {
        name: "id", value: "body-id"
      },
      {
        name: "class", value: "body-class"
      }
    ]
  },
  {
    input: "testNodeInIframe()",
    output: "<p>",
    tagName: "P",
    attrs: []
  },
  {
    input: "testDocumentElement()",
    output: '<html lang="en-US" dir="ltr">',
    tagName: "HTML",
    attrs: [
      {
        name: "lang",
        value: "en-US"
      },
      {
        name: "dir",
        value: "ltr"
      }
    ]
  }
];

function test() {
  Task.spawn(function*() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    let toolbox = gDevTools.getToolbox(hud.target);

    // Loading the inspector panel at first, to make it possible to listen for
    // new node selections
    yield toolbox.selectTool("inspector");
    let inspector = toolbox.getCurrentPanel();
    yield toolbox.selectTool("webconsole");

    info("Iterating over the test data");
    for (let data of TEST_DATA) {
      let [result] = yield jsEval(data.input, hud, {text: data.output});
      let {msg} = yield getWidgetAndMessage(result);

      let inspectorIcon = msg.querySelector(".open-inspector");
      ok(inspectorIcon, "Inspector icon found in the ElementNode widget");

      info("Clicking on the inspector icon and waiting for the " +
           "inspector to be selected");
      let onInspectorSelected = toolbox.once("inspector-selected");
      let onInspectorUpdated = inspector.once("inspector-updated");
      let onNewNode = toolbox.selection.once("new-node-front");
      let onNodeHighlight = toolbox.once("node-highlight");

      EventUtils.synthesizeMouseAtCenter(inspectorIcon, {},
        inspectorIcon.ownerDocument.defaultView);
      yield onInspectorSelected;
      yield onInspectorUpdated;
      yield onNodeHighlight;
      let nodeFront = yield onNewNode;

      ok(true, "Inspector selected and new node got selected");

      is(nodeFront.tagName, data.tagName, "The correct node was highlighted");

      let attrs = nodeFront.attributes;
      for (let i in data.attrs) {
        is(attrs[i].name, data.attrs[i].name,
           "The correct node was highlighted");
        is(attrs[i].value, data.attrs[i].value,
           "The correct node was highlighted");
      }

      info("Unhighlight the node by moving away from the markup view");
      let onNodeUnhighlight = toolbox.once("node-unhighlight");
      let btn = inspector.toolbox.doc.querySelector(".toolbox-dock-button");
      EventUtils.synthesizeMouseAtCenter(btn, {type: "mousemove"},
        inspector.toolbox.doc.defaultView);
      yield onNodeUnhighlight;

      info("Switching back to the console");
      yield toolbox.selectTool("webconsole");
    }
  }).then(finishTest);
}

function jsEval(input, hud, message) {
  info("Executing '" + input + "' in the web console");

  hud.jsterm.clearOutput();
  hud.jsterm.execute(input);

  return waitForMessages({
    webconsole: hud,
    messages: [message]
  });
}

function* getWidgetAndMessage(result) {
  info("Getting the output ElementNode widget");

  let msg = [...result.matched][0];
  let widget = [...msg._messageObject.widgets][0];
  ok(widget, "ElementNode widget found in the output");

  info("Waiting for the ElementNode widget to be linked to the inspector");
  yield widget.linkToInspector();

  return {widget: widget, msg: msg};
}
