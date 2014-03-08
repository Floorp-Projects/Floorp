/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that inspector links in webconsole outputs for DOM Nodes highlight
// the actual DOM Nodes on hover

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-output-dom-elements.html";

function test() {
  Task.spawn(function*() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    let toolbox = gDevTools.getToolbox(hud.target);

    // Loading the inspector panel at first, to make it possible to listen for
    // new node selections
    yield toolbox.loadTool("inspector");
    let inspector = toolbox.getPanel("inspector");

    info("Executing 'testNode()' in the web console to output a DOM Node");
    let [result] = yield jsEval("testNode()", hud, {
      text: '<p some-attribute="some-value">'
    });

    let elementNodeWidget = yield getWidget(result);

    let nodeFront = yield hoverOverWidget(elementNodeWidget, toolbox);
    let attrs = nodeFront.attributes;
    is(nodeFront.tagName, "P", "The correct node was highlighted");
    is(attrs[0].name, "some-attribute", "The correct node was highlighted");
    is(attrs[0].value, "some-value", "The correct node was highlighted");
  }).then(finishTest);
}

function jsEval(input, hud, message) {
  hud.jsterm.execute(input);
  return waitForMessages({
    webconsole: hud,
    messages: [message]
  });
}

function* getWidget(result) {
  info("Getting the output ElementNode widget");

  let msg = [...result.matched][0];
  let elementNodeWidget = [...msg._messageObject.widgets][0];
  ok(elementNodeWidget, "ElementNode widget found in the output");

  info("Waiting for the ElementNode widget to be linked to the inspector");
  yield elementNodeWidget.linkToInspector();

  return elementNodeWidget;
}

function* hoverOverWidget(widget, toolbox) {
  info("Hovering over the output to highlight the node");

  let onHighlight = toolbox.once("node-highlight");
  EventUtils.sendMouseEvent({type: "mouseover"}, widget.element,
    widget.element.ownerDocument.defaultView);
  let nodeFront = yield onHighlight;
  ok(true, "The highlighter was shown on a node");
  return nodeFront;
}
