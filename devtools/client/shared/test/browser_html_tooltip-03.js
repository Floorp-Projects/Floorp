/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip autofocus configuration option.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = `data:text/xml;charset=UTF-8,<?xml version="1.0"?>
  <?xml-stylesheet href="chrome://global/skin/global.css"?>
  <?xml-stylesheet href="chrome://devtools/skin/tooltips.css"?>
  <window xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
   title="Tooltip test">
    <vbox flex="1">
      <hbox id="box1" flex="1">
        <textbox></textbox>
      </hbox>
      <hbox id="box2" flex="1">test2</hbox>
      <hbox id="box3" flex="1">
        <textbox id="box3-input"></textbox>
      </hbox>
      <hbox id="box4" flex="1">
        <textbox id="box4-input"></textbox>
      </hbox>
    </vbox>
  </window>`;

const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

add_task(function* () {
  yield addTab("about:blank");
  let [, , doc] = yield createHost("bottom", TEST_URI);

  yield testNoAutoFocus(doc);
  yield testAutoFocus(doc);
  yield testAutoFocusPreservesFocusChange(doc);
});

function* testNoAutoFocus(doc) {
  yield focusNode(doc, "#box4-input");
  is(getFocusedDocument(doc), doc, "Focus is in the XUL document");

  info("Test a tooltip without autofocus will not take focus");
  let tooltip = yield createTooltip(doc, false);

  yield showTooltip(tooltip, doc.getElementById("box1"));
  is(getFocusedDocument(doc), doc, "Focus is still in the XUL document");
  ok(doc.activeElement.closest("#box4-input"), "Focus is in the #box4-input");

  yield hideTooltip(tooltip);
  yield blurNode(doc, "#box4-input");
}

function* testAutoFocus(doc) {
  yield focusNode(doc, "#box4-input");
  is(getFocusedDocument(doc), doc, "Focus is in the XUL document");

  info("Test autofocus tooltip takes focus when displayed, " +
    "and restores the focus when hidden");
  let tooltip = yield createTooltip(doc, true);

  yield showTooltip(tooltip, doc.getElementById("box1"));
  is(getFocusedDocument(doc), tooltip.panel.ownerDocument,
    "Focus is in the tooltip document");

  yield hideTooltip(tooltip);
  is(getFocusedDocument(doc), doc, "Focus is back in the XUL document");
  ok(doc.activeElement.closest("#box4-input"), "Focus is in the #box4-input");

  info("Blur the textbox before moving to the next test to reset the state.");
  yield blurNode(doc, "#box4-input");
}

function* testAutoFocusPreservesFocusChange(doc) {
  yield focusNode(doc, "#box4-input");
  is(getFocusedDocument(doc), doc, "Focus is in the XUL document");

  info("Test autofocus tooltip takes focus when displayed, " +
    "but does not try to restore the active element if it is not focused when hidden");
  let tooltip = yield createTooltip(doc, true);

  yield showTooltip(tooltip, doc.getElementById("box1"));
  is(getFocusedDocument(doc), tooltip.panel.ownerDocument,
    "Focus is in the tooltip document");

  info("Move the focus to #box3-input while the tooltip is displayed");
  yield focusNode(doc, "#box3-input");
  is(getFocusedDocument(doc), doc, "Focus is back in the XUL document");
  ok(doc.activeElement.closest("#box3-input"), "Focus is in the #box3-input");

  yield hideTooltip(tooltip);
  is(getFocusedDocument(doc), doc, "Focus is still in the XUL document");

  ok(doc.activeElement.closest("#box3-input"), "Focus is still in the #box3-input");

  info("Blur the textbox before moving to the next test to reset the state.");
  yield blurNode(doc, "#box3-input");
}

function getFocusedDocument(doc) {
  let activeElement = doc.activeElement;
  while (activeElement && activeElement.contentDocument) {
    activeElement = activeElement.contentDocument.activeElement;
  }
  return activeElement.ownerDocument;
}

/**
 * Fpcus the node corresponding to the provided selector in the provided document. Returns
 * a promise that will resolve when receiving the focus event on the node.
 */
function focusNode(doc, selector) {
  let node = doc.querySelector(selector);
  let onFocus = once(node, "focus");
  node.focus();
  return onFocus;
}

/**
 * Blur the node corresponding to the provided selector in the provided document. Returns
 * a promise that will resolve when receiving the blur event on the node.
 */
function blurNode(doc, selector) {
  let node = doc.querySelector(selector);
  let onBlur = once(node, "blur");
  node.blur();
  return onBlur;
}

/**
 * Create an HTMLTooltip instance with the provided autofocus setting.
 *
 * @param {Document} doc
 *        Document in which the tooltip should be created
 * @param {Boolean} autofocus
 * @return {Promise} promise that will resolve the HTMLTooltip instance created when the
 *         tooltip content will be ready.
 */
function* createTooltip(doc, autofocus) {
  let tooltip = new HTMLTooltip({doc}, {autofocus});
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "50px";
  yield tooltip.setContent(div, 150, 50);
  return tooltip;
}
