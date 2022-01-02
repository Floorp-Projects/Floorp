/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that graph widgets works properly.

const LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");

add_task(async function() {
  await addTab("about:blank");
  await performTest();
  gBrowser.removeCurrentTab();
  finish();
});

async function performTest() {
  const { host, doc } = await createHost();
  doc.body.setAttribute(
    "style",
    "position: fixed; width: 100%; height: 100%; margin: 0;"
  );

  const graph = new LineGraphWidget(doc.body, "fps");

  let readyEventEmitted;
  graph.once("ready", () => {
    readyEventEmitted = true;
  });

  await graph.ready();
  ok(readyEventEmitted, "The 'ready' event should have been emitted");

  testGraph(host, graph);

  await graph.destroy();
  host.destroy();
}

function testGraph(host, graph) {
  ok(
    graph._container.classList.contains("line-graph-widget-container"),
    "The correct graph container was created."
  );
  ok(
    graph._canvas.classList.contains("line-graph-widget-canvas"),
    "The correct graph container was created."
  );

  const bounds = host.frame.getBoundingClientRect();

  is(
    graph.width,
    bounds.width * window.devicePixelRatio,
    "The graph has the correct width."
  );
  is(
    graph.height,
    bounds.height * window.devicePixelRatio,
    "The graph has the correct height."
  );

  ok(
    graph._cursor.x === null,
    "The graph's cursor X coordinate is initially null."
  );
  ok(
    graph._cursor.y === null,
    "The graph's cursor Y coordinate is initially null."
  );

  ok(
    graph._selection.start === null,
    "The graph's selection start value is initially null."
  );
  ok(
    graph._selection.end === null,
    "The graph's selection end value is initially null."
  );

  ok(
    graph._selectionDragger.origin === null,
    "The graph's dragger origin value is initially null."
  );
  ok(
    graph._selectionDragger.anchor.start === null,
    "The graph's dragger anchor start value is initially null."
  );
  ok(
    graph._selectionDragger.anchor.end === null,
    "The graph's dragger anchor end value is initially null."
  );

  ok(
    graph._selectionResizer.margin === null,
    "The graph's resizer margin value is initially null."
  );
}
