/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that line graphs properly create the gutter and tooltips.

const TEST_DATA = [{ delta: 112, value: 48 }, { delta: 213, value: 59 }, { delta: 313, value: 60 }, { delta: 413, value: 59 }, { delta: 530, value: 59 }, { delta: 646, value: 58 }, { delta: 747, value: 60 }, { delta: 863, value: 48 }, { delta: 980, value: 37 }, { delta: 1097, value: 30 }, { delta: 1213, value: 29 }, { delta: 1330, value: 23 }, { delta: 1430, value: 10 }, { delta: 1534, value: 17 }, { delta: 1645, value: 20 }, { delta: 1746, value: 22 }, { delta: 1846, value: 39 }, { delta: 1963, value: 26 }, { delta: 2080, value: 27 }, { delta: 2197, value: 35 }, { delta: 2312, value: 47 }, { delta: 2412, value: 53 }, { delta: 2514, value: 60 }, { delta: 2630, value: 37 }, { delta: 2730, value: 36 }, { delta: 2830, value: 37 }, { delta: 2946, value: 36 }, { delta: 3046, value: 40 }, { delta: 3163, value: 47 }, { delta: 3280, value: 41 }, { delta: 3380, value: 35 }, { delta: 3480, value: 27 }, { delta: 3580, value: 39 }, { delta: 3680, value: 42 }, { delta: 3780, value: 49 }, { delta: 3880, value: 55 }, { delta: 3980, value: 60 }, { delta: 4080, value: 60 }, { delta: 4180, value: 60 }];
var LineGraphWidget = require("devtools/shared/widgets/LineGraphWidget");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  let graph = new LineGraphWidget(doc.body, { metric: "fps" });

  yield testGraph(graph);

  yield graph.destroy();
  host.destroy();
}

function* testGraph(graph) {
  info("Should be able to set the graph data before waiting for the ready event.");

  yield graph.setDataWhenReady(TEST_DATA);
  ok(graph.hasData(), "Data was set successfully.");

  is(graph._gutter.hidden, false,
    "The gutter should not be hidden because the tooltips have arrows.");
  is(graph._maxTooltip.hidden, false,
    "The max tooltip should not be hidden.");
  is(graph._avgTooltip.hidden, false,
    "The avg tooltip should not be hidden.");
  is(graph._minTooltip.hidden, false,
    "The min tooltip should not be hidden.");

  is(graph._maxTooltip.getAttribute("with-arrows"), "true",
    "The maximum tooltip has the correct 'with-arrows' attribute.");
  is(graph._avgTooltip.getAttribute("with-arrows"), "true",
    "The average tooltip has the correct 'with-arrows' attribute.");
  is(graph._minTooltip.getAttribute("with-arrows"), "true",
    "The minimum tooltip has the correct 'with-arrows' attribute.");

  is(graph._maxTooltip.querySelector("[text=info]").textContent, "max",
    "The maximum tooltip displays the correct info.");
  is(graph._avgTooltip.querySelector("[text=info]").textContent, "avg",
    "The average tooltip displays the correct info.");
  is(graph._minTooltip.querySelector("[text=info]").textContent, "min",
    "The minimum tooltip displays the correct info.");

  is(graph._maxTooltip.querySelector("[text=value]").textContent, "60",
    "The maximum tooltip displays the correct value.");
  is(graph._avgTooltip.querySelector("[text=value]").textContent, "41.72",
    "The average tooltip displays the correct value.");
  is(graph._minTooltip.querySelector("[text=value]").textContent, "10",
    "The minimum tooltip displays the correct value.");

  is(graph._maxTooltip.querySelector("[text=metric]").textContent, "fps",
    "The maximum tooltip displays the correct metric.");
  is(graph._avgTooltip.querySelector("[text=metric]").textContent, "fps",
    "The average tooltip displays the correct metric.");
  is(graph._minTooltip.querySelector("[text=metric]").textContent, "fps",
    "The minimum tooltip displays the correct metric.");

  is(parseInt(graph._maxTooltip.style.top), 22,
    "The maximum tooltip is positioned correctly.");
  is(parseInt(graph._avgTooltip.style.top), 61,
    "The average tooltip is positioned correctly.");
  is(parseInt(graph._minTooltip.style.top), 128,
    "The minimum tooltip is positioned correctly.");

  is(parseInt(graph._maxGutterLine.style.top), 22,
    "The maximum gutter line is positioned correctly.");
  is(parseInt(graph._avgGutterLine.style.top), 61,
    "The average gutter line is positioned correctly.");
  is(parseInt(graph._minGutterLine.style.top), 128,
    "The minimum gutter line is positioned correctly.");
}
