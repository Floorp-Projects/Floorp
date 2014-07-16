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

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  inspector.markup._frame.focus();

  info("Checking that pressing escape cancels edits");
  yield testEscapeCancels(inspector);

  info("Checking that pressing F2 commits edits");
  yield testF2Commits(inspector);

  info("Checking that editing the <body> element works like other nodes");
  yield testBody(inspector);

  info("Checking that editing the <head> element works like other nodes");
  yield testHead(inspector);

  info("Checking that editing the <html> element works like other nodes");
  yield testDocumentElement(inspector);

  info("Checking (again) that editing the <html> element works like other nodes");
  yield testDocumentElement2(inspector);
});

function testEscapeCancels(inspector) {
  let def = promise.defer();
  let node = getNode(SELECTOR);

  selectNode(SELECTOR, inspector).then(() => {
    inspector.markup.htmlEditor.on("popupshown", function onPopupShown() {
      inspector.markup.htmlEditor.off("popupshown", onPopupShown);

      ok(inspector.markup.htmlEditor._visible, "HTML Editor is visible");
      is(node.outerHTML, OLD_HTML, "The node is starting with old HTML.");

      inspector.markup.htmlEditor.on("popuphidden", function onPopupHidden() {
        inspector.markup.htmlEditor.off("popuphidden", onPopupHidden);
        ok(!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");

        let node = getNode(SELECTOR);
        is(node.outerHTML, OLD_HTML, "Escape cancels edits");
        def.resolve();
      });

      inspector.markup.htmlEditor.editor.setText(NEW_HTML);

      EventUtils.sendKey("ESCAPE", inspector.markup.htmlEditor.doc.defaultView);
    });

    EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  });

  return def.promise;
}

function testF2Commits(inspector) {
  let def = promise.defer();
  let node = getNode(SELECTOR);

  inspector.markup.htmlEditor.on("popupshown", function onPopupShown() {
    inspector.markup.htmlEditor.off("popupshown", onPopupShown);

    ok(inspector.markup.htmlEditor._visible, "HTML Editor is visible");
    is(node.outerHTML, OLD_HTML, "The node is starting with old HTML.");

    inspector.once("markupmutation", (e, aMutations) => {
      ok(!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");

      let node = getNode(SELECTOR);
      is(node.outerHTML, NEW_HTML, "F2 commits edits - the node has new HTML.");
      def.resolve();
    });

    inspector.markup.htmlEditor.editor.setText(NEW_HTML);
    EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  });

  inspector.markup._frame.contentDocument.documentElement.focus();
  EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);

  return def.promise;
}

function* testBody(inspector) {
  let body = getNode("body");
  let bodyHTML = '<body id="updated"><p></p></body>';
  let bodyFront = yield getNodeFront("body", inspector);
  let doc = content.document;

  let mutated = inspector.once("markupmutation");
  inspector.markup.updateNodeOuterHTML(bodyFront, bodyHTML, body.outerHTML);

  let mutations = yield mutated;

  is(getNode("body").outerHTML, bodyHTML, "<body> HTML has been updated");
  is(doc.querySelectorAll("head").length, 1, "no extra <head>s have been added");

  yield inspector.once("inspector-updated");
}

function* testHead(inspector) {
  let head = getNode("head");
  let headHTML = '<head id="updated"><title>New Title</title><script>window.foo="bar";</script></head>';
  let headFront = yield getNodeFront("head", inspector);
  let doc = content.document;

  let mutated = inspector.once("markupmutation");
  inspector.markup.updateNodeOuterHTML(headFront, headHTML, head.outerHTML);

  let mutations = yield mutated;

  is(doc.title, "New Title", "New title has been added");
  is(doc.defaultView.foo, undefined, "Script has not been executed");
  is(doc.querySelector("head").outerHTML, headHTML, "<head> HTML has been updated");
  is(doc.querySelectorAll("body").length, 1, "no extra <body>s have been added");

  yield inspector.once("inspector-updated");
}

function* testDocumentElement(inspector) {
  let doc = content.document;
  let docElement = doc.documentElement;
  let docElementHTML = '<html id="updated" foo="bar"><head><title>Updated from document element</title><script>window.foo="bar";</script></head><body><p>Hello</p></body></html>';
  let docElementFront = yield inspector.markup.walker.documentElement();

  let mutated = inspector.once("markupmutation");
  inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML, docElement.outerHTML);

  let mutations = yield mutated;

  is(doc.title, "Updated from document element", "New title has been added");
  is(doc.defaultView.foo, undefined, "Script has not been executed");
  is(doc.documentElement.id, "updated", "<html> ID has been updated");
  is(doc.documentElement.className, "", "<html> class has been updated");
  is(doc.documentElement.getAttribute("foo"), "bar", "<html> attribute has been updated");
  is(doc.documentElement.outerHTML, docElementHTML, "<html> HTML has been updated");
  is(doc.querySelectorAll("head").length, 1, "no extra <head>s have been added");
  is(doc.querySelectorAll("body").length, 1, "no extra <body>s have been added");
  is(doc.body.textContent, "Hello", "document.body.textContent has been updated");
}

function* testDocumentElement2(inspector) {
  let doc = content.document;
  let docElement = doc.documentElement;
  let docElementHTML = '<html class="updated" id="somethingelse"><head><title>Updated again from document element</title><script>window.foo="bar";</script></head><body><p>Hello again</p></body></html>';
  let docElementFront = yield inspector.markup.walker.documentElement();

  let mutated = inspector.once("markupmutation");
  inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML, docElement.outerHTML);

  let mutations = yield mutated;

  is(doc.title, "Updated again from document element", "New title has been added");
  is(doc.defaultView.foo, undefined, "Script has not been executed");
  is(doc.documentElement.id, "somethingelse", "<html> ID has been updated");
  is(doc.documentElement.className, "updated", "<html> class has been updated");
  is(doc.documentElement.getAttribute("foo"), null, "<html> attribute has been removed");
  is(doc.documentElement.outerHTML, docElementHTML, "<html> HTML has been updated");
  is(doc.querySelectorAll("head").length, 1, "no extra <head>s have been added");
  is(doc.querySelectorAll("body").length, 1, "no extra <body>s have been added");
  is(doc.body.textContent, "Hello again", "document.body.textContent has been updated");
}
