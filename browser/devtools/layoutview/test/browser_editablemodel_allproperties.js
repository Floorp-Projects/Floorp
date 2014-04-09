
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
  // TODO: Closing the toolbox in this test leaks - bug 994314
  // yield gDevTools.closeToolbox(target);
});

addTest("When all properties are set on the node editing one should work",
function*() {
  let node = doc.getElementById("div1");
  node.style.padding = "5px";
  let view = yield selectNode(node);

  let span = view.document.querySelector(".padding.bottom > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("7", {}, view);
  yield waitForUpdate();

  is(editor.value, "7", "Should have the right value in the editor.");
  is(getStyle(node, "padding-bottom"), "7px", "Should have updated the padding");

  EventUtils.synthesizeKey("VK_RETURN", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "padding-bottom"), "7px", "Should be the right padding.")
  is(span.textContent, 7, "Should have the right value in the box model.");
});

addTest("When all properties are set on the node editing one should work",
function*() {
  let node = doc.getElementById("div1");
  node.style.padding = "5px";
  let view = yield selectNode(node);

  let span = view.document.querySelector(".padding.left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("8", {}, view);
  yield waitForUpdate();

  is(editor.value, "8", "Should have the right value in the editor.");
  is(getStyle(node, "padding-left"), "8px", "Should have updated the padding");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "padding-left"), "5px", "Should be the right padding.")
  is(span.textContent, 5, "Should have the right value in the box model.");
});

addTest("When all properties are set on the node deleting one should work",
function*() {
  let node = doc.getElementById("div1");
  node.style.padding = "5px";
  let view = yield selectNode(node);

  let span = view.document.querySelector(".padding.left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view);
  yield waitForUpdate();

  is(editor.value, "", "Should have the right value in the editor.");
  is(getStyle(node, "padding-left"), "", "Should have updated the padding");

  EventUtils.synthesizeKey("VK_RETURN", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "padding-left"), "", "Should be the right padding.")
  is(span.textContent, 3, "Should have the right value in the box model.");
});

addTest("When all properties are set on the node deleting one then cancelling should work",
function*() {
  let node = doc.getElementById("div1");
  node.style.padding = "5px";
  let view = yield selectNode(node);

  let span = view.document.querySelector(".padding.left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view);
  yield waitForUpdate();

  is(editor.value, "", "Should have the right value in the editor.");
  is(getStyle(node, "padding-left"), "", "Should have updated the padding");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "padding-left"), "5px", "Should be the right padding.")
  is(span.textContent, 5, "Should have the right value in the box model.");
});
