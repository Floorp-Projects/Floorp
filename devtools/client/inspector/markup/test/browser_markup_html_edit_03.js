/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that outerHTML editing keybindings work as expected and that *special*
// elements like <html>, <body> and <head> can be edited correctly.

const TEST_URL = "data:text/html," +
  "<!DOCTYPE html>" +
  "<head><meta charset='utf-8' /></head>" +
  "<body>" +
  "<div id=\"keyboard\"></div>" +
  "</body>" +
  "</html>";
const SELECTOR = "#keyboard";
const OLD_HTML = '<div id="keyboard"></div>';
const NEW_HTML = '<div id="keyboard">Edited</div>';

requestLongerTimeout(2);

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);

  inspector.markup._frame.focus();

  info("Check that pressing escape cancels edits");
  await testEscapeCancels(inspector, testActor);

  info("Check that pressing F2 commits edits");
  await testF2Commits(inspector, testActor);

  info("Check that editing the <body> element works like other nodes");
  await testBody(inspector, testActor);

  info("Check that editing the <head> element works like other nodes");
  await testHead(inspector, testActor);

  info("Check that editing the <html> element works like other nodes");
  await testDocumentElement(inspector, testActor);

  info("Check (again) that editing the <html> element works like other nodes");
  await testDocumentElement2(inspector, testActor);
});

async function testEscapeCancels(inspector, testActor) {
  await selectNode(SELECTOR, inspector);

  const onHtmlEditorCreated = once(inspector.markup, "begin-editing");
  EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  await onHtmlEditorCreated;
  ok(inspector.markup.htmlEditor._visible, "HTML Editor is visible");

  is((await testActor.getProperty(SELECTOR, "outerHTML")), OLD_HTML,
     "The node is starting with old HTML.");

  inspector.markup.htmlEditor.editor.setText(NEW_HTML);

  const onEditorHiddem = once(inspector.markup.htmlEditor, "popuphidden");
  EventUtils.sendKey("ESCAPE", inspector.markup.htmlEditor.doc.defaultView);
  await onEditorHiddem;
  ok(!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");

  is((await testActor.getProperty(SELECTOR, "outerHTML")), OLD_HTML,
     "Escape cancels edits");
}

async function testF2Commits(inspector, testActor) {
  const onEditorShown = once(inspector.markup.htmlEditor, "popupshown");
  inspector.markup._frame.contentDocument.documentElement.focus();
  EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  await onEditorShown;
  ok(inspector.markup.htmlEditor._visible, "HTML Editor is visible");

  is((await testActor.getProperty(SELECTOR, "outerHTML")), OLD_HTML,
     "The node is starting with old HTML.");

  const onMutations = inspector.once("markupmutation");
  inspector.markup.htmlEditor.editor.setText(NEW_HTML);
  EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  await onMutations;

  ok(!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");

  is((await testActor.getProperty(SELECTOR, "outerHTML")), NEW_HTML,
     "F2 commits edits - the node has new HTML.");
}

async function testBody(inspector, testActor) {
  const currentBodyHTML = await testActor.getProperty("body", "outerHTML");
  const bodyHTML = '<body id="updated"><p></p></body>';
  const bodyFront = await getNodeFront("body", inspector);

  const onUpdated = inspector.once("inspector-updated");
  const onReselected = inspector.markup.once("reselectedonremoved");
  await inspector.markup.updateNodeOuterHTML(bodyFront, bodyHTML,
                                             currentBodyHTML);
  await onReselected;
  await onUpdated;

  const newBodyHTML = await testActor.getProperty("body", "outerHTML");
  is(newBodyHTML, bodyHTML, "<body> HTML has been updated");

  const headsNum = await testActor.getNumberOfElementMatches("head");
  is(headsNum, 1, "no extra <head>s have been added");
}

async function testHead(inspector, testActor) {
  await selectNode("head", inspector);

  const currentHeadHTML = await testActor.getProperty("head", "outerHTML");
  const headHTML = "<head id=\"updated\"><title>New Title</title>" +
                 "<script>window.foo=\"bar\";</script></head>";
  const headFront = await getNodeFront("head", inspector);

  const onUpdated = inspector.once("inspector-updated");
  const onReselected = inspector.markup.once("reselectedonremoved");
  await inspector.markup.updateNodeOuterHTML(headFront, headHTML,
                                             currentHeadHTML);
  await onReselected;
  await onUpdated;

  is((await testActor.eval("document.title")), "New Title",
     "New title has been added");
  is((await testActor.eval("window.foo")), undefined,
     "Script has not been executed");
  is((await testActor.getProperty("head", "outerHTML")), headHTML,
     "<head> HTML has been updated");
  is((await testActor.getNumberOfElementMatches("body")), 1,
     "no extra <body>s have been added");
}

async function testDocumentElement(inspector, testActor) {
  const currentDocElementOuterHMTL = await testActor.eval(
    "document.documentElement.outerHMTL");
  const docElementHTML = "<html id=\"updated\" foo=\"bar\"><head>" +
                       "<title>Updated from document element</title>" +
                       "<script>window.foo=\"bar\";</script></head><body>" +
                       "<p>Hello</p></body></html>";
  const docElementFront = await inspector.markup.walker.documentElement();

  const onReselected = inspector.markup.once("reselectedonremoved");
  await inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML,
    currentDocElementOuterHMTL);
  await onReselected;

  is((await testActor.eval("document.title")),
     "Updated from document element", "New title has been added");
  is((await testActor.eval("window.foo")),
     undefined, "Script has not been executed");
  is((await testActor.getAttribute("html", "id")),
     "updated", "<html> ID has been updated");
  is((await testActor.getAttribute("html", "class")),
     null, "<html> class has been updated");
  is((await testActor.getAttribute("html", "foo")),
     "bar", "<html> attribute has been updated");
  is((await testActor.getProperty("html", "outerHTML")),
     docElementHTML, "<html> HTML has been updated");
  is((await testActor.getNumberOfElementMatches("head")),
     1, "no extra <head>s have been added");
  is((await testActor.getNumberOfElementMatches("body")),
     1, "no extra <body>s have been added");
  is((await testActor.getProperty("body", "textContent")),
     "Hello", "document.body.textContent has been updated");
}

async function testDocumentElement2(inspector, testActor) {
  const currentDocElementOuterHMTL = await testActor.eval(
    "document.documentElement.outerHMTL");
  const docElementHTML = "<html id=\"somethingelse\" class=\"updated\"><head>" +
                       "<title>Updated again from document element</title>" +
                       "<script>window.foo=\"bar\";</script></head><body>" +
                       "<p>Hello again</p></body></html>";
  const docElementFront = await inspector.markup.walker.documentElement();

  const onReselected = inspector.markup.once("reselectedonremoved");
  inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML,
    currentDocElementOuterHMTL);
  await onReselected;

  is((await testActor.eval("document.title")),
     "Updated again from document element", "New title has been added");
  is((await testActor.eval("window.foo")),
     undefined, "Script has not been executed");
  is((await testActor.getAttribute("html", "id")),
     "somethingelse", "<html> ID has been updated");
  is((await testActor.getAttribute("html", "class")),
     "updated", "<html> class has been updated");
  is((await testActor.getAttribute("html", "foo")),
     null, "<html> attribute has been removed");
  is((await testActor.getProperty("html", "outerHTML")),
     docElementHTML, "<html> HTML has been updated");
  is((await testActor.getNumberOfElementMatches("head")),
     1, "no extra <head>s have been added");
  is((await testActor.getNumberOfElementMatches("body")),
     1, "no extra <body>s have been added");
  is((await testActor.getProperty("body", "textContent")),
     "Hello again", "document.body.textContent has been updated");
}
