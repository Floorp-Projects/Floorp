/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the variable inspection popup has inspector links for DOMNode
 * properties and that the popup closes when the link is clicked
 */

const TAB_URL = EXAMPLE_URL + "doc_domnode-variables.html";

function test() {
  Task.spawn(function*() {
    let [tab,, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let bubble = win.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;
    let toolbox = gDevTools.getToolbox(panel.target);

    function getDomNodeInTooltip(propertyName) {
      let domNodeProperties = tooltip.querySelectorAll(".token-domnode");
      for (let prop of domNodeProperties) {
        let propName = prop.parentNode.querySelector(".name");
        if (propName.getAttribute("value") === propertyName) {
          ok(true, "DOMNode " + propertyName + " was found in the tooltip");
          return prop;
        }
      }
      ok(false, "DOMNode " + propertyName + " wasn't found in the tooltip");
    }

    callInTab(tab, "start");
    yield waitForSourceAndCaretAndScopes(panel, ".html", 19);

    // Inspect the div DOM variable.
    yield openVarPopup(panel, { line: 17, ch: 38 }, true);
    let property = getDomNodeInTooltip("firstElementChild");

    // Simulate mouseover on the property value
    let highlighted = once(toolbox, "node-highlight");
    EventUtils.sendMouseEvent({ type: "mouseover" }, property,
      property.ownerDocument.defaultView);
    yield highlighted;
    ok(true, "The node-highlight event was fired on hover of the DOMNode");

    // Simulate a click on the "select in inspector" button
    let button = property.parentNode.querySelector(".variables-view-open-inspector");
    ok(button, "The select-in-inspector button is present");
    let inspectorSelected = once(toolbox, "inspector-selected");
    EventUtils.sendMouseEvent({ type: "mousedown" }, button,
      button.ownerDocument.defaultView);
    yield inspectorSelected;
    ok(true, "The inspector got selected when clicked on the select-in-inspector");

    // Make sure the inspector's initialization is finalized before ending the test
    // Listening to the event *after* triggering the switch to the inspector isn't
    // a problem as the inspector is asynchronously loaded.
    yield once(toolbox.getPanel("inspector"), "inspector-updated");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
