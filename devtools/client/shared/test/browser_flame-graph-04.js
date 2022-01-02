/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that text metrics in the flame graph widget work properly.

const HTML_NS = "http://www.w3.org/1999/xhtml";
const { ELLIPSIS } = require("devtools/shared/l10n");
const { FlameGraph } = require("devtools/client/shared/widgets/FlameGraph");
const {
  FLAME_GRAPH_BLOCK_TEXT_FONT_SIZE,
} = require("devtools/client/shared/widgets/FlameGraph");
const {
  FLAME_GRAPH_BLOCK_TEXT_FONT_FAMILY,
} = require("devtools/client/shared/widgets/FlameGraph");

add_task(async function() {
  await addTab("about:blank");
  await performTest();
  gBrowser.removeCurrentTab();
});

async function performTest() {
  const { host, doc } = await createHost();
  const graph = new FlameGraph(doc.body, 1);
  await graph.ready();

  testGraph(graph);

  await graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  is(
    graph._averageCharWidth,
    getAverageCharWidth(),
    "The average char width was calculated correctly."
  );
  is(
    graph._overflowCharWidth,
    getCharWidth(ELLIPSIS),
    "The ellipsis char width was calculated correctly."
  );

  const text = "This text is maybe overflowing";
  const text1000px = graph._getFittedText(text, 1000);
  const text50px = graph._getFittedText(text, 50);
  const text10px = graph._getFittedText(text, 10);
  const text1px = graph._getFittedText(text, 1);

  is(
    graph._getTextWidthApprox(text),
    getAverageCharWidth() * text.length,
    "The approximate width was calculated correctly."
  );

  info("Text at 1000px width: " + text1000px);
  info("Text at 50px width  : " + text50px);
  info("Text at 10px width  : " + text10px);
  info("Text at 1px width   : " + text1px);

  is(text1000px, text, "The fitted text for 1000px width is correct.");

  isnot(text50px, text, "The fitted text for 50px width is correct (1).");

  ok(
    text50px.includes(ELLIPSIS),
    "The fitted text for 50px width is correct (2)."
  );

  is(
    graph._getFittedText(text, FLAME_GRAPH_BLOCK_TEXT_FONT_SIZE + 1),
    ELLIPSIS,
    "The fitted text for text font size width is correct."
  );

  is(
    graph._getFittedText(text, 1),
    "",
    "The fitted text for 1px width is correct."
  );
}

function getAverageCharWidth() {
  let letterWidthsSum = 0;

  const start = " ".charCodeAt(0);
  const end = "z".charCodeAt(0) + 1;

  for (let i = start; i < end; i++) {
    const char = String.fromCharCode(i);
    letterWidthsSum += getCharWidth(char);
  }

  return letterWidthsSum / (end - start);
}

function getCharWidth(char) {
  const canvas = document.createElementNS(HTML_NS, "canvas");
  const ctx = canvas.getContext("2d");

  const fontSize = FLAME_GRAPH_BLOCK_TEXT_FONT_SIZE;
  const fontFamily = FLAME_GRAPH_BLOCK_TEXT_FONT_FAMILY;
  ctx.font = fontSize + "px " + fontFamily;

  return ctx.measureText(char).width;
}
