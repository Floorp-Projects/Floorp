/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that inspector links in the webconsole output for DOM Nodes do not try
// to highlight or select nodes once they have been detached

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-output-dom-elements.html";

const TEST_DATA = [
  {
    // The first test shouldn't be returning the body element as this is the
    // default selected node, so re-selecting it won't fire the
    // inspector-updated event
    input: "testNode()",
    output: '<p some-attribute="some-value">'
  },
  {
    input: "testSvgNode()",
    output: '<clipPath>'
  },
  {
    input: "testBodyNode()",
    output: '<body class="body-class" id="body-id">'
  },
  {
    input: "testNodeInIframe()",
    output: "<p>"
  },
  {
    input: "testDocumentElement()",
    output: '<html dir="ltr" lang="en-US">'
  }
];

const PREF = "devtools.webconsole.persistlog";

function test() {
  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF));

  Task.spawn(function* () {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    let toolbox = gDevTools.getToolbox(hud.target);

    info("Executing the test data");
    let widgets = [];
    for (let data of TEST_DATA) {
      let [result] = yield jsEval(data.input, hud, {text: data.output});
      let {widget} = yield getWidgetAndMessage(result);
      widgets.push(widget);
    }

    info("Reloading the page");
    yield reloadPage();

    info("Iterating over the ElementNode widgets");
    for (let widget of widgets) {
      // Verify that openNodeInInspector rejects since the associated dom node
      // doesn't exist anymore
      yield widget.openNodeInInspector().then(() => {
        ok(false, "The openNodeInInspector promise resolved");
      }, () => {
        ok(true, "The openNodeInInspector promise rejected as expected");
      });
      yield toolbox.selectTool("webconsole");

      // Verify that highlightDomNode rejects too, for the same reason
      yield widget.highlightDomNode().then(() => {
        ok(false, "The highlightDomNode promise resolved");
      }, () => {
        ok(true, "The highlightDomNode promise rejected as expected");
      });
    }
  }).then(finishTest);
}

function jsEval(input, hud, message) {
  info("Executing '" + input + "' in the web console");
  hud.jsterm.execute(input);
  return waitForMessages({
    webconsole: hud,
    messages: [message]
  });
}

function* getWidgetAndMessage(result) {
  info("Getting the output ElementNode widget");

  let msg = [...result.matched][0];
  let widget = [...msg._messageObject.widgets][0];
  ok(widget, "ElementNode widget found in the output");

  info("Waiting for the ElementNode widget to be linked to the inspector");
  yield widget.linkToInspector();

  return {widget: widget, msg: msg};
}

function reloadPage() {
  let def = promise.defer();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    def.resolve();
  }, true);
  content.location.reload();
  return def.promise;
}
