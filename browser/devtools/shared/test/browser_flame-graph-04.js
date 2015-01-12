/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that text metrics in the flame graph widget work properly.

let HTML_NS = "http://www.w3.org/1999/xhtml";
let FLAME_GRAPH_BLOCK_TEXT_FONT_SIZE = 9; // px
let FLAME_GRAPH_BLOCK_TEXT_FONT_FAMILY = "sans-serif";
let {ViewHelpers} = Cu.import("resource:///modules/devtools/ViewHelpers.jsm", {});
let {FlameGraph} = Cu.import("resource:///modules/devtools/FlameGraph.jsm", {});
let {DOMHelpers} = Cu.import("resource:///modules/devtools/DOMHelpers.jsm", {});
let {Promise} = devtools.require("resource://gre/modules/Promise.jsm");
let {Hosts} = devtools.require("devtools/framework/toolbox-hosts");

let L10N = new ViewHelpers.L10N();

let test = Task.async(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
  finish();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  let graph = new FlameGraph(doc.body, 1);
  yield graph.ready();

  testGraph(graph);

  graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  is(graph._averageCharWidth, getAverageCharWidth(),
    "The average char width was calculated correctly.");
  is(graph._overflowCharWidth, getCharWidth(L10N.ellipsis),
    "The ellipsis char width was calculated correctly.");

  is(graph._getTextWidthApprox("This text is maybe overflowing"),
    getAverageCharWidth() * 30,
    "The approximate width was calculated correctly.");

  is(graph._getFittedText("This text is maybe overflowing", 1000),
    "This text is maybe overflowing",
    "The fitted text for 1000px width is correct.");

  isnot(graph._getFittedText("This text is maybe overflowing", 100),
    "This text is maybe overflowing",
    "The fitted text for 100px width is correct (1).");

  ok(graph._getFittedText("This text is maybe overflowing", 100)
    .contains(L10N.ellipsis),
    "The fitted text for 100px width is correct (2).");

  is(graph._getFittedText("This text is maybe overflowing", 10),
    L10N.ellipsis,
    "The fitted text for 10px width is correct.");

  is(graph._getFittedText("This text is maybe overflowing", 1),
    "",
    "The fitted text for 1px width is correct.");
}

function getAverageCharWidth() {
  let letterWidthsSum = 0;
  let start = 32; // space
  let end = 123; // "z"

  for (let i = start; i < end; i++) {
    let char = String.fromCharCode(i);
    letterWidthsSum += getCharWidth(char);
  }

  return letterWidthsSum / (end - start);
}

function getCharWidth(char) {
  let canvas = document.createElementNS(HTML_NS, "canvas");
  let ctx = canvas.getContext("2d");

  let fontSize = FLAME_GRAPH_BLOCK_TEXT_FONT_SIZE;
  let fontFamily = FLAME_GRAPH_BLOCK_TEXT_FONT_FAMILY;
  ctx.font = fontSize + "px " + fontFamily;

  return ctx.measureText(char).width;
}
