/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getStyle(node, property) {
  return node.style.getPropertyValue(property);
}

let doc;
let inspector;

let test = Task.async(function*() {
  waitForExplicitFinish();

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

  gBrowser.removeCurrentTab();
  finish();
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

addTest("Test that entering units works",
function*() {
  let node = doc.getElementById("div1");
  is(getStyle(node, "padding-top"), "", "Should have the right padding");
  let view = yield selectNode(node);

  let span = view.document.querySelector(".padding.top > span");
  is(span.textContent, 3, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "3px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("1", {}, view);
  yield waitForUpdate();
  EventUtils.synthesizeKey("e", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "padding-top"), "", "An invalid value is handled cleanly");

  EventUtils.synthesizeKey("m", {}, view);
  yield waitForUpdate();

  is(editor.value, "1em", "Should have the right value in the editor.");
  is(getStyle(node, "padding-top"), "1em", "Should have updated the padding.");

  EventUtils.synthesizeKey("VK_RETURN", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "padding-top"), "1em", "Should be the right padding.")
  is(span.textContent, 16, "Should have the right value in the box model.");
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

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "padding-left"), "5px", "Should be the right padding.")
  is(span.textContent, 5, "Should have the right value in the box model.");
});

addTest("Test that we pick up the value from a higher style rule",
function*() {
  let node = doc.getElementById("div2");
  is(getStyle(node, "border-bottom-width"), "", "Should have the right border-bottom-width");
  let view = yield selectNode(node);

  let span = view.document.querySelector(".border.bottom > span");
  is(span.textContent, 16, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "1em", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("0", {}, view);
  yield waitForUpdate();

  is(editor.value, "0", "Should have the right value in the editor.");
  is(getStyle(node, "border-bottom-width"), "0px", "Should have updated the border.");

  EventUtils.synthesizeKey("VK_RETURN", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "border-bottom-width"), "0px", "Should be the right border-bottom-width.")
  is(span.textContent, 0, "Should have the right value in the box model.");
});

addTest("Test that shorthand properties are parsed correctly",
function*() {
  let node = doc.getElementById("div3");
  is(getStyle(node, "padding-right"), "", "Should have the right padding");
  let view = yield selectNode(node);

  let span = view.document.querySelector(".padding.right > span");
  is(span.textContent, 32, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "2em", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_RETURN", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "padding-right"), "", "Should be the right padding.")
  is(span.textContent, 32, "Should have the right value in the box model.");
});

addTest("Test that adding a border applies a border style when necessary",
function*() {
  let node = doc.getElementById("div1");
  is(getStyle(node, "border-top-width"), "", "Should have the right border");
  is(getStyle(node, "border-top-style"), "", "Should have the right border");
  let view = yield selectNode(node);

  let span = view.document.querySelector(".border.top > span");
  is(span.textContent, 0, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view);
  let editor = view.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "0", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("1", {}, view);
  yield waitForUpdate();

  is(editor.value, "1", "Should have the right value in the editor.");
  is(getStyle(node, "border-top-width"), "1px", "Should have the right border");
  is(getStyle(node, "border-top-style"), "solid", "Should have the right border");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view);
  yield waitForUpdate();

  is(getStyle(node, "border-top-width"), "", "Should be the right padding.")
  is(getStyle(node, "border-top-style"), "", "Should have the right border");
  is(span.textContent, 0, "Should have the right value in the box model.");
});
