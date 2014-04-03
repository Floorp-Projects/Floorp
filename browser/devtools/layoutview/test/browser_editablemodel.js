/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getStyle(node, property) {
  return node.style.getPropertyValue(property);
}

let doc;
let inspector;

let test = asyncTest(function*() {
  let style = "div { margin: 10px; padding: 3px } #div1 { margin-top: 5px } #div2 { border-bottom: 1em solid black; } #div3 { padding: 2em; }";
  let html = "<style>" + style + "</style><div id='div1'></div><div id='div2'></div><div id='div3'></div>"

  let content = yield loadTab("data:text/html," + encodeURIComponent(html));
  doc = content.document;

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  inspector = toolbox.getCurrentPanel();

  inspector.sidebar.select("layoutview");
  yield inspector.sidebar.once("layoutview-ready");
  yield runTests();
  yield gDevTools.closeToolbox(toolbox);
});

addTest("Test that editing margin dynamically updates the document, pressing escape cancels the changes",
function*() {
  let node = doc.getElementById("div1");
  is(getStyle(node, "margin-top"), "", "Should be no margin-top on the element.")
  let view = yield selectNode(node);

  let span = view.document.querySelector(".margin.top > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("3", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "margin-top"), "3px", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "margin-top"), "", "Should be no margin-top on the element.")
  is(span.textContent, 5, "Should have the right value in the box model.");
});

addTest("Test that arrow keys work correctly and pressing enter commits the changes",
function*() {
  let node = doc.getElementById("div1");
  is(getStyle(node, "margin-left"), "", "Should be no margin-top on the element.")
  let view = yield selectNode(node);

  let span = view.document.querySelector(".margin.left > span");
  is(span.textContent, 10, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "10px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_UP", {}, view);
  yield waitForUpdate();

  is(editor.value, "11px", "Should have the right value in the editor.");
  is(getStyle(node, "margin-left"), "11px", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_DOWN", {}, view);
  yield waitForUpdate();

  is(editor.value, "10px", "Should have the right value in the editor.");
  is(getStyle(node, "margin-left"), "10px", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_UP", { shiftKey: true }, view);
  yield waitForUpdate();

  is(editor.value, "20px", "Should have the right value in the editor.");
  is(getStyle(node, "margin-left"), "20px", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_RETURN", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "margin-left"), "20px", "Should be the right margin-top on the element.")
  is(span.textContent, 20, "Should have the right value in the box model.");
});

addTest("Test that deleting the value removes the property but escape undoes that",
function*() {
  let node = doc.getElementById("div1");
  is(getStyle(node, "margin-left"), "20px", "Should be the right margin-top on the element.")
  let view = yield selectNode(node);

  let span = view.document.querySelector(".margin.left > span");
  is(span.textContent, 20, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "20px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view);
  yield waitForUpdate();

  is(editor.value, "", "Should have the right value in the editor.");
  is(getStyle(node, "margin-left"), "", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "margin-left"), "20px", "Should be the right margin-top on the element.")
  is(span.textContent, 20, "Should have the right value in the box model.");
});

addTest("Test that deleting the value removes the property",
function*() {
  let node = doc.getElementById("div1");
  node.style.marginRight = "15px";
  let view = yield selectNode(node);

  let span = view.document.querySelector(".margin.right > span");
  is(span.textContent, 15, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "15px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view);
  yield waitForUpdate();

  is(editor.value, "", "Should have the right value in the editor.");
  is(getStyle(node, "margin-right"), "", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_RETURN", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "margin-right"), "", "Should be the right margin-top on the element.")
  is(span.textContent, 10, "Should have the right value in the box model.");
});
