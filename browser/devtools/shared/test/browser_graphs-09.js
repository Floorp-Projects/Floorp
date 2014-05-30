/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that line graphs properly create the gutter and tooltips.

const TEST_DATA = {"112":48,"213":59,"313":60,"413":59,"530":59,"646":58,"747":60,"863":48,"980":37,"1097":30,"1213":29,"1330":23,"1430":10,"1534":17,"1645":20,"1746":22,"1846":39,"1963":26,"2080":27,"2197":35,"2312":47,"2412":53,"2514":60,"2630":37,"2730":36,"2830":37,"2946":36,"3046":40,"3163":47,"3280":41,"3380":35,"3480":27,"3580":39,"3680":42,"3780":49,"3880":55,"3980":60,"4080":60,"4180":60};
let {LineGraphWidget} = Cu.import("resource:///modules/devtools/Graphs.jsm", {});
let {DOMHelpers} = Cu.import("resource:///modules/devtools/DOMHelpers.jsm", {});
let {Promise} = devtools.require("resource://gre/modules/Promise.jsm");
let {Hosts} = devtools.require("devtools/framework/toolbox-hosts");

let test = Task.async(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
  finish();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  let graph = new LineGraphWidget(doc.body, "fps");
  yield graph.once("ready");

  testGraph(graph);

  graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  graph.setData(TEST_DATA);

  is(graph._maxTooltip.querySelector("[text=info]").textContent, "max",
    "The maximum tooltip displays the correct info.");
  is(graph._avgTooltip.querySelector("[text=info]").textContent, "avg",
    "The average tooltip displays the correct info.");
  is(graph._minTooltip.querySelector("[text=info]").textContent, "min",
    "The minimum tooltip displays the correct info.");

  is(graph._maxTooltip.querySelector("[text=value]").textContent, "60",
    "The maximum tooltip displays the correct value.");
  is(graph._avgTooltip.querySelector("[text=value]").textContent, "41",
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
