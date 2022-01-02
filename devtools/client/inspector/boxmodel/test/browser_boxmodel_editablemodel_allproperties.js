/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing box model values when all values are set

const TEST_URI =
  "<style>" +
  "div { margin: 10px; padding: 3px }" +
  "#div1 { margin-top: 5px }" +
  "#div2 { border-bottom: 1em solid black; }" +
  "#div3 { padding: 2em; }" +
  "</style>" +
  "<div id='div1'></div><div id='div2'></div><div id='div3'></div>";

add_task(async function() {
  const tab = await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const { inspector, boxmodel } = await openLayoutView();

  const browser = tab.linkedBrowser;
  await testEditing(inspector, boxmodel, browser);
  await testEditingAndCanceling(inspector, boxmodel, browser);
  await testDeleting(inspector, boxmodel, browser);
  await testDeletingAndCanceling(inspector, boxmodel, browser);
});

async function testEditing(inspector, boxmodel, browser) {
  info("When all properties are set on the node editing one should work");

  await setStyle(browser, "#div1", "padding", "5px");
  await waitForUpdate(inspector);

  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-padding.boxmodel-bottom > span"
  );
  await waitForElementTextContent(span, "5");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("7", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "7", "Should have the right value in the editor.");
  is(
    await getStyle(browser, "#div1", "padding-bottom"),
    "7px",
    "Should have updated the padding"
  );

  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is(
    await getStyle(browser, "#div1", "padding-bottom"),
    "7px",
    "Should be the right padding."
  );
  await waitForElementTextContent(span, "7");
}

async function testEditingAndCanceling(inspector, boxmodel, browser) {
  info(
    "When all properties are set on the node editing one and then " +
      "cancelling with ESCAPE should work"
  );

  await setStyle(browser, "#div1", "padding", "5px");
  await waitForUpdate(inspector);

  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-padding.boxmodel-left > span"
  );
  await waitForElementTextContent(span, "5");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("8", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "8", "Should have the right value in the editor.");
  is(
    await getStyle(browser, "#div1", "padding-left"),
    "8px",
    "Should have updated the padding"
  );

  EventUtils.synthesizeKey("VK_ESCAPE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(
    await getStyle(browser, "#div1", "padding-left"),
    "5px",
    "Should be the right padding."
  );
  await waitForElementTextContent(span, "5");
}

async function testDeleting(inspector, boxmodel, browser) {
  info("When all properties are set on the node deleting one should work");

  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-padding.boxmodel-left > span"
  );
  await waitForElementTextContent(span, "5");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is(
    await getStyle(browser, "#div1", "padding-left"),
    "",
    "Should have updated the padding"
  );

  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is(
    await getStyle(browser, "#div1", "padding-left"),
    "",
    "Should be the right padding."
  );
  await waitForElementTextContent(span, "3");
}

async function testDeletingAndCanceling(inspector, boxmodel, browser) {
  info(
    "When all properties are set on the node deleting one then cancelling " +
      "should work"
  );

  await setStyle(browser, "#div1", "padding", "5px");
  await waitForUpdate(inspector);

  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-padding.boxmodel-left > span"
  );
  await waitForElementTextContent(span, "5");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is(
    await getStyle(browser, "#div1", "padding-left"),
    "",
    "Should have updated the padding"
  );

  EventUtils.synthesizeKey("VK_ESCAPE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(
    await getStyle(browser, "#div1", "padding-left"),
    "5px",
    "Should be the right padding."
  );
  await waitForElementTextContent(span, "5");
}
