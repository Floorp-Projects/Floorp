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
  <?xml-stylesheet href="chrome://devtools/skin/common.css"?>
  <window xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
   title="Tooltip test">
    <vbox flex="1">
      <hbox id="box1" flex="1">test1</hbox>
      <hbox id="box2" flex="1">test2</hbox>
      <hbox id="box3" flex="1">test3</hbox>
      <hbox id="box4" flex="1">
        <textbox id="box4-input"></textbox>
      </hbox>
    </vbox>
  </window>`;

const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

add_task(function* () {
  yield addTab("about:blank");
  let [,, doc] = yield createHost("bottom", TEST_URI);

  yield testTooltipWithAutoFocus(doc);
  yield testTooltipWithoutAutoFocus(doc);
});

function* testTooltipWithAutoFocus(doc) {
  info("Test a tooltip with autofocus takes focus when displayed");
  let textbox = doc.querySelector("textbox");

  info("Focus a XUL textbox");
  let onInputFocus = once(textbox, "focus");
  EventUtils.synthesizeMouseAtCenter(textbox, {}, doc.defaultView);
  yield onInputFocus;

  is(getFocusedDocument(doc), doc, "Focus is in the XUL document");

  let tooltip = new HTMLTooltip({doc}, {autofocus: true});
  let tooltipNode = getTooltipContent(doc);
  yield tooltip.setContent(tooltipNode, 150, 50);

  yield showTooltip(tooltip, doc.getElementById("box1"));
  is(getFocusedDocument(doc), tooltipNode.ownerDocument,
    "Focus is in the tooltip document");

  yield hideTooltip(tooltip);
}

function* testTooltipWithoutAutoFocus(doc) {
  info("Test a tooltip can be closed by clicking outside");
  let textbox = doc.querySelector("textbox");

  info("Focus a XUL textbox");
  let onInputFocus = once(textbox, "focus");
  EventUtils.synthesizeMouseAtCenter(textbox, {}, doc.defaultView);
  yield onInputFocus;

  is(getFocusedDocument(doc), doc, "Focus is in the XUL document");

  let tooltip = new HTMLTooltip({doc}, {autofocus: false});
  let tooltipNode = getTooltipContent(doc);
  yield tooltip.setContent(tooltipNode, 150, 50);

  yield showTooltip(tooltip, doc.getElementById("box1"));
  is(getFocusedDocument(doc), doc, "Focus is still in the XUL document");

  yield hideTooltip(tooltip);
}

function getFocusedDocument(doc) {
  let activeElement = doc.activeElement;
  while (activeElement && activeElement.contentDocument) {
    activeElement = activeElement.contentDocument.activeElement;
  }
  return activeElement.ownerDocument;
}

function getTooltipContent(doc) {
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "50px";
  div.style.boxSizing = "border-box";
  return div;
}
