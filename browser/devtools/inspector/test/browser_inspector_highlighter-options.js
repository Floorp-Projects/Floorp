/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that the box-model highlighter supports configuration options

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<body style='padding:2em;'>" +
                 "<div style='width:100px;height:100px;padding:2em;border:.5em solid black;margin:1em;'>test</div>" +
                 "</body>";

// Test data format:
// - desc: a string that will be output to the console.
// - options: json object to be passed as options to the highlighter.
// - checkHighlighter: a generator (async) function that should check the
//   highlighter is correct.
const TEST_DATA = [
  {
    desc: "Guides and infobar should be shown by default",
    options: {},
    checkHighlighter: function*(toolbox) {
      let hidden = yield getAttribute("box-model-nodeinfobar-container", "hidden", toolbox);
      ok(!hidden, "Node infobar is visible");

      hidden = yield getAttribute("box-model-elements", "hidden", toolbox);
      ok(!hidden, "SVG container is visible");

      for (let side of ["top", "right", "bottom", "left"]) {
        hidden = yield getAttribute("box-model-guide-" + side, "hidden", toolbox);
        ok(!hidden, side + " guide is visible");
      }
    }
  },
  {
    desc: "All regions should be shown by default",
    options: {},
    checkHighlighter: function*(toolbox) {
      for (let region of ["margin", "border", "padding", "content"]) {
        let points = yield getAttribute("box-model-" + region, "points", toolbox);
        ok(points, "Region " + region + " has set coordinates");
      }
    }
  },
  {
    desc: "Guides can be hidden",
    options: {hideGuides: true},
    checkHighlighter: function*(toolbox) {
      for (let side of ["top", "right", "bottom", "left"]) {
        let hidden = yield getAttribute("box-model-guide-" + side, "hidden", toolbox);
        is(hidden, "true", side + " guide has been hidden");
      }
    }
  },
  {
    desc: "Infobar can be hidden",
    options: {hideInfoBar: true},
    checkHighlighter: function*(toolbox) {
      let hidden = yield getAttribute("box-model-nodeinfobar-container", "hidden", toolbox);
      is(hidden, "true", "nodeinfobar has been hidden");
    }
  },
  {
    desc: "One region only can be shown (1)",
    options: {showOnly: "content"},
    checkHighlighter: function*(toolbox) {
      let points = yield getAttribute("box-model-margin", "points", toolbox);
      ok(!points, "margin region is hidden");

      points = yield getAttribute("box-model-border", "points", toolbox);
      ok(!points, "border region is hidden");

      points = yield getAttribute("box-model-padding", "points", toolbox);
      ok(!points, "padding region is hidden");

      points = yield getAttribute("box-model-content", "points", toolbox);
      ok(points, "content region is shown");
    }
  },
  {
    desc: "One region only can be shown (2)",
    options: {showOnly: "margin"},
    checkHighlighter: function*(toolbox) {
      let points = yield getAttribute("box-model-margin", "points", toolbox);
      ok(points, "margin region is shown");

      points = yield getAttribute("box-model-border", "points", toolbox);
      ok(!points, "border region is hidden");

      points = yield getAttribute("box-model-padding", "points", toolbox);
      ok(!points, "padding region is hidden");

      points = yield getAttribute("box-model-content", "points", toolbox);
      ok(!points, "content region is hidden");
    }
  },
  {
    desc: "Guides can be drawn around a given region (1)",
    options: {region: "padding"},
    checkHighlighter: function*(toolbox) {
      let topY1 = yield getAttribute("box-model-guide-top", "y1", toolbox);
      let rightX1 = yield getAttribute("box-model-guide-right", "x1", toolbox);
      let bottomY1 = yield getAttribute("box-model-guide-bottom", "y1", toolbox);
      let leftX1 = yield getAttribute("box-model-guide-left", "x1", toolbox);

      let points = yield getAttribute("box-model-padding", "points", toolbox);
      points = points.split(" ").map(xy => xy.split(","));

      is(Math.ceil(topY1), points[0][1], "Top guide's y1 is correct");
      is(Math.floor(rightX1), points[1][0], "Right guide's x1 is correct");
      is(Math.floor(bottomY1), points[2][1], "Bottom guide's y1 is correct");
      is(Math.ceil(leftX1), points[3][0], "Left guide's x1 is correct");
    }
  },
  {
    desc: "Guides can be drawn around a given region (2)",
    options: {region: "margin"},
    checkHighlighter: function*(toolbox) {
      let topY1 = yield getAttribute("box-model-guide-top", "y1", toolbox);
      let rightX1 = yield getAttribute("box-model-guide-right", "x1", toolbox);
      let bottomY1 = yield getAttribute("box-model-guide-bottom", "y1", toolbox);
      let leftX1 = yield getAttribute("box-model-guide-left", "x1", toolbox);

      let points = yield getAttribute("box-model-margin", "points", toolbox);

      points = points.split(" ").map(xy => xy.split(","));
      is(Math.ceil(topY1), points[0][1], "Top guide's y1 is correct");
      is(Math.floor(rightX1), points[1][0], "Right guide's x1 is correct");
      is(Math.floor(bottomY1), points[2][1], "Bottom guide's y1 is correct");
      is(Math.ceil(leftX1), points[3][0], "Left guide's x1 is correct");
    }
  }
];

add_task(function*() {
  let {inspector, toolbox} = yield openInspectorForURL(TEST_URL);

  let divFront = yield getNodeFront("div", inspector);

  for (let {desc, options, checkHighlighter} of TEST_DATA) {
    info("Running test: " + desc);

    info("Show the box-model highlighter with options " + options);
    yield toolbox.highlighter.showBoxModel(divFront, options);

    yield checkHighlighter(toolbox);

    info("Hide the box-model highlighter");
    yield toolbox.highlighter.hideBoxModel();
  }
});

function* getAttribute(nodeID, name, toolbox) {
  let actorID = getHighlighterActorID(toolbox);
  let {data: value} = yield executeInContent("Test:GetHighlighterAttribute",
                                             {nodeID, name, actorID});
  return value;
}
