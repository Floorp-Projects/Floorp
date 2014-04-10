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
