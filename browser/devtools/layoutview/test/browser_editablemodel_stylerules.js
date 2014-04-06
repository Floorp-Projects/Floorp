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
