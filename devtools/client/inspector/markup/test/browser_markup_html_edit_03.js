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

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);

  inspector.markup._frame.focus();

  info("Check that pressing escape cancels edits");
  yield testEscapeCancels(inspector, testActor);

  info("Check that pressing F2 commits edits");
  yield testF2Commits(inspector, testActor);

  info("Check that editing the <body> element works like other nodes");
  yield testBody(inspector, testActor);

  info("Check that editing the <head> element works like other nodes");
  yield testHead(inspector, testActor);

  info("Check that editing the <html> element works like other nodes");
  yield testDocumentElement(inspector, testActor);

  info("Check (again) that editing the <html> element works like other nodes");
  yield testDocumentElement2(inspector, testActor);
});

function* testEscapeCancels(inspector, testActor) {
  yield selectNode(SELECTOR, inspector);

  let onEditorShown = once(inspector.markup.htmlEditor, "popupshown");
  EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  yield onEditorShown;
  ok(inspector.markup.htmlEditor._visible, "HTML Editor is visible");

  is((yield testActor.getProperty(SELECTOR, "outerHTML")), OLD_HTML,
     "The node is starting with old HTML.");

  inspector.markup.htmlEditor.editor.setText(NEW_HTML);

  let onEditorHiddem = once(inspector.markup.htmlEditor, "popuphidden");
  EventUtils.sendKey("ESCAPE", inspector.markup.htmlEditor.doc.defaultView);
  yield onEditorHiddem;
  ok(!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");

  is((yield testActor.getProperty(SELECTOR, "outerHTML")), OLD_HTML,
     "Escape cancels edits");
}

function* testF2Commits(inspector, testActor) {
  let onEditorShown = once(inspector.markup.htmlEditor, "popupshown");
  inspector.markup._frame.contentDocument.documentElement.focus();
  EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  yield onEditorShown;
  ok(inspector.markup.htmlEditor._visible, "HTML Editor is visible");

  is((yield testActor.getProperty(SELECTOR, "outerHTML")), OLD_HTML,
     "The node is starting with old HTML.");

  let onMutations = inspector.once("markupmutation");
  inspector.markup.htmlEditor.editor.setText(NEW_HTML);
  EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  yield onMutations;

  ok(!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");

  is((yield testActor.getProperty(SELECTOR, "outerHTML")), NEW_HTML,
     "F2 commits edits - the node has new HTML.");
}

function* testBody(inspector, testActor) {
  let currentBodyHTML = yield testActor.getProperty("body", "outerHTML");
  let bodyHTML = '<body id="updated"><p></p></body>';
  let bodyFront = yield getNodeFront("body", inspector);

  let onUpdated = inspector.once("inspector-updated");
  let onReselected = inspector.markup.once("reselectedonremoved");
  yield inspector.markup.updateNodeOuterHTML(bodyFront, bodyHTML,
                                             currentBodyHTML);
  yield onReselected;
  yield onUpdated;

  let newBodyHTML = yield testActor.getProperty("body", "outerHTML");
  is(newBodyHTML, bodyHTML, "<body> HTML has been updated");

  let headsNum = yield testActor.getNumberOfElementMatches("head");
  is(headsNum, 1, "no extra <head>s have been added");
}

function* testHead(inspector, testActor) {
  yield selectNode("head", inspector);

  let currentHeadHTML = yield testActor.getProperty("head", "outerHTML");
  let headHTML = "<head id=\"updated\"><title>New Title</title>" +
                 "<script>window.foo=\"bar\";</script></head>";
  let headFront = yield getNodeFront("head", inspector);

  let onUpdated = inspector.once("inspector-updated");
  let onReselected = inspector.markup.once("reselectedonremoved");
  yield inspector.markup.updateNodeOuterHTML(headFront, headHTML,
                                             currentHeadHTML);
  yield onReselected;
  yield onUpdated;

  is((yield testActor.eval("content.document.title")), "New Title",
     "New title has been added");
  is((yield testActor.eval("content.foo")), undefined,
     "Script has not been executed");
  is((yield testActor.getProperty("head", "outerHTML")), headHTML,
     "<head> HTML has been updated");
  is((yield testActor.getNumberOfElementMatches("body")), 1,
     "no extra <body>s have been added");
}

function* testDocumentElement(inspector, testActor) {
  let currentDocElementOuterHMTL = yield testActor.eval(
    "content.document.documentElement.outerHMTL");
  let docElementHTML = "<html id=\"updated\" foo=\"bar\"><head>" +
                       "<title>Updated from document element</title>" +
                       "<script>window.foo=\"bar\";</script></head><body>" +
                       "<p>Hello</p></body></html>";
  let docElementFront = yield inspector.markup.walker.documentElement();

  let onReselected = inspector.markup.once("reselectedonremoved");
  yield inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML,
    currentDocElementOuterHMTL);
  yield onReselected;

  is((yield testActor.eval("content.document.title")),
     "Updated from document element", "New title has been added");
  is((yield testActor.eval("content.foo")),
     undefined, "Script has not been executed");
  is((yield testActor.getAttribute("html", "id")),
     "updated", "<html> ID has been updated");
  is((yield testActor.getAttribute("html", "class")),
     null, "<html> class has been updated");
  is((yield testActor.getAttribute("html", "foo")),
     "bar", "<html> attribute has been updated");
  is((yield testActor.getProperty("html", "outerHTML")),
     docElementHTML, "<html> HTML has been updated");
  is((yield testActor.getNumberOfElementMatches("head")),
     1, "no extra <head>s have been added");
  is((yield testActor.getNumberOfElementMatches("body")),
     1, "no extra <body>s have been added");
  is((yield testActor.getProperty("body", "textContent")),
     "Hello", "document.body.textContent has been updated");
}

function* testDocumentElement2(inspector, testActor) {
  let currentDocElementOuterHMTL = yield testActor.eval(
    "content.document.documentElement.outerHMTL");
  let docElementHTML = "<html id=\"somethingelse\" class=\"updated\"><head>" +
                       "<title>Updated again from document element</title>" +
                       "<script>window.foo=\"bar\";</script></head><body>" +
                       "<p>Hello again</p></body></html>";
  let docElementFront = yield inspector.markup.walker.documentElement();

  let onReselected = inspector.markup.once("reselectedonremoved");
  inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML,
    currentDocElementOuterHMTL);
  yield onReselected;

  is((yield testActor.eval("content.document.title")),
     "Updated again from document element", "New title has been added");
  is((yield testActor.eval("content.foo")),
     undefined, "Script has not been executed");
  is((yield testActor.getAttribute("html", "id")),
     "somethingelse", "<html> ID has been updated");
  is((yield testActor.getAttribute("html", "class")),
     "updated", "<html> class has been updated");
  is((yield testActor.getAttribute("html", "foo")),
     null, "<html> attribute has been removed");
  is((yield testActor.getProperty("html", "outerHTML")),
     docElementHTML, "<html> HTML has been updated");
  is((yield testActor.getNumberOfElementMatches("head")),
     1, "no extra <head>s have been added");
  is((yield testActor.getNumberOfElementMatches("body")),
     1, "no extra <body>s have been added");
  is((yield testActor.getProperty("body", "textContent")),
     "Hello again", "document.body.textContent has been updated");
}
