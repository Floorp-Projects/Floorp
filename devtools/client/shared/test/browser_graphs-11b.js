/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that bar graph's legend items handle mouseover/mouseout.

var BarGraphWidget = require("devtools/client/shared/widgets/BarGraphWidget");

const CATEGORIES = [
  { color: "#46afe3", label: "Foo" },
  { color: "#eb5368", label: "Bar" },
  { color: "#70bf53", label: "Baz" }
];

add_task(function*() {
  yield addTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  doc.body.setAttribute("style", "position: fixed; width: 100%; height: 100%; margin: 0;");

  let graph = new BarGraphWidget(doc.body, 1);
  graph.fixedWidth = 200;
  graph.fixedHeight = 100;

  yield graph.once("ready");
  yield testGraph(graph);

  yield graph.destroy();
  host.destroy();
}

function* testGraph(graph) {
  graph.format = CATEGORIES;
  graph.dataOffsetX = 1000;
  graph.setData([{
    delta: 1100, values: [0, 2, 3]
  }, {
    delta: 1200, values: [1, 0, 2]
  }, {
    delta: 1300, values: [2, 1, 0]
  }, {
    delta: 1400, values: [0, 3, 1]
  }, {
    delta: 1500, values: [3, 0, 2]
  }, {
    delta: 1600, values: [3, 2, 0]
  }]);

  is(graph._blocksBoundingRects.toSource(), "[{type:1, start:0, end:33.33333333333333, top:70, bottom:100}, {type:2, start:0, end:33.33333333333333, top:24, bottom:69}, {type:0, start:34.33333333333333, end:66.66666666666666, top:85, bottom:100}, {type:2, start:34.33333333333333, end:66.66666666666666, top:54, bottom:84}, {type:0, start:67.66666666666666, end:100, top:70, bottom:100}, {type:1, start:67.66666666666666, end:100, top:54, bottom:69}, {type:1, start:101, end:133.33333333333331, top:55, bottom:100}, {type:2, start:101, end:133.33333333333331, top:39, bottom:54}, {type:0, start:134.33333333333331, end:166.66666666666666, top:55, bottom:100}, {type:2, start:134.33333333333331, end:166.66666666666666, top:24, bottom:54}, {type:0, start:167.66666666666666, end:200, top:55, bottom:100}, {type:1, start:167.66666666666666, end:200, top:24, bottom:54}]",
    "The correct blocks bounding rects were calculated for the bar graph.");

  let legendItems = graph._document.querySelectorAll(".bar-graph-widget-legend-item");
  is(legendItems.length, 3,
    "Three legend items should exist in the entire graph.");

  yield testLegend(graph, 0, {
    highlights: "[{type:0, start:34.33333333333333, end:66.66666666666666, top:85, bottom:100}, {type:0, start:67.66666666666666, end:100, top:70, bottom:100}, {type:0, start:134.33333333333331, end:166.66666666666666, top:55, bottom:100}, {type:0, start:167.66666666666666, end:200, top:55, bottom:100}]",
    selection: "({start:34.33333333333333, end:200})",
    leftmost: "({type:0, start:34.33333333333333, end:66.66666666666666, top:85, bottom:100})",
    rightmost: "({type:0, start:167.66666666666666, end:200, top:55, bottom:100})"
  });
  yield testLegend(graph, 1, {
    highlights: "[{type:1, start:0, end:33.33333333333333, top:70, bottom:100}, {type:1, start:67.66666666666666, end:100, top:54, bottom:69}, {type:1, start:101, end:133.33333333333331, top:55, bottom:100}, {type:1, start:167.66666666666666, end:200, top:24, bottom:54}]",
    selection: "({start:0, end:200})",
    leftmost: "({type:1, start:0, end:33.33333333333333, top:70, bottom:100})",
    rightmost: "({type:1, start:167.66666666666666, end:200, top:24, bottom:54})"
  });
  yield testLegend(graph, 2, {
    highlights: "[{type:2, start:0, end:33.33333333333333, top:24, bottom:69}, {type:2, start:34.33333333333333, end:66.66666666666666, top:54, bottom:84}, {type:2, start:101, end:133.33333333333331, top:39, bottom:54}, {type:2, start:134.33333333333331, end:166.66666666666666, top:24, bottom:54}]",
    selection: "({start:0, end:166.66666666666666})",
    leftmost: "({type:2, start:0, end:33.33333333333333, top:24, bottom:69})",
    rightmost: "({type:2, start:134.33333333333331, end:166.66666666666666, top:24, bottom:54})"
  });
}

function* testLegend(graph, index, { highlights, selection, leftmost, rightmost }) {
  // Hover.

  let legendItems = graph._document.querySelectorAll(".bar-graph-widget-legend-item");
  let colorBlock = legendItems[index].querySelector("[view=color]");

  let debounced = graph.once("legend-hover");
  graph._onLegendMouseOver({ target: colorBlock });
  ok(!graph.hasMask(), "The graph shouldn't get highlights immediately.");

  let [type, rects] = yield debounced;
  ok(graph.hasMask(), "The graph should now have highlights.");

  is(type, index,
    "The legend item was correctly hovered.");
  is(rects.toSource(), highlights,
    "The legend item highlighted the correct regions.");

  // Unhover.

  let unhovered = graph.once("legend-unhover");
  graph._onLegendMouseOut();
  ok(!graph.hasMask(), "The graph shouldn't have highlights anymore.");

  yield unhovered;
  ok(true, "The 'legend-mouseout' event was emitted.");

  // Select.

  let selected = graph.once("legend-selection");
  graph._onLegendMouseDown(mockEvent(colorBlock));
  ok(graph.hasSelection(), "The graph should now have a selection.");
  is(graph.getSelection().toSource(), selection, "The graph has a correct selection.");

  let [left, right] = yield selected;
  is(left.toSource(), leftmost, "The correct leftmost data block was found.");
  is(right.toSource(), rightmost, "The correct rightmost data block was found.");

  // Deselect.

  graph.dropSelection();
}

function mockEvent(node) {
  return {
    target: node,
    preventDefault: () => {},
    stopPropagation: () => {}
  };
}
