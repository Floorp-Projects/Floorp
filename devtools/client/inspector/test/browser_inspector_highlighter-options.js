/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that the box-model highlighter supports configuration options

const TEST_URL = `
  <body style="padding:2em;">
    <div style="width:100px;height:100px;padding:2em;
                border:.5em solid black;margin:1em;">test</div>
  </body>
`;

// Test data format:
// - desc: a string that will be output to the console.
// - options: json object to be passed as options to the highlighter.
// - checkHighlighter: a generator (async) function that should check the
//   highlighter is correct.
const TEST_DATA = [
  {
    desc: "Guides and infobar should be shown by default",
    options: {},
    checkHighlighter: async function(testActor) {
      let hidden = await testActor.getHighlighterNodeAttribute(
        "box-model-infobar-container", "hidden");
      ok(!hidden, "Node infobar is visible");

      hidden = await testActor.getHighlighterNodeAttribute(
        "box-model-elements", "hidden");
      ok(!hidden, "SVG container is visible");

      for (const side of ["top", "right", "bottom", "left"]) {
        hidden = await testActor.getHighlighterNodeAttribute(
          "box-model-guide-" + side, "hidden");
        ok(!hidden, side + " guide is visible");
      }
    }
  },
  {
    desc: "All regions should be shown by default",
    options: {},
    checkHighlighter: async function(testActor) {
      for (const region of ["margin", "border", "padding", "content"]) {
        const {d} = await testActor.getHighlighterRegionPath(region);
        ok(d, "Region " + region + " has set coordinates");
      }
    }
  },
  {
    desc: "Guides can be hidden",
    options: {hideGuides: true},
    checkHighlighter: async function(testActor) {
      for (const side of ["top", "right", "bottom", "left"]) {
        const hidden = await testActor.getHighlighterNodeAttribute(
          "box-model-guide-" + side, "hidden");
        is(hidden, "true", side + " guide has been hidden");
      }
    }
  },
  {
    desc: "Infobar can be hidden",
    options: {hideInfoBar: true},
    checkHighlighter: async function(testActor) {
      const hidden = await testActor.getHighlighterNodeAttribute(
        "box-model-infobar-container", "hidden");
      is(hidden, "true", "infobar has been hidden");
    }
  },
  {
    desc: "One region only can be shown (1)",
    options: {showOnly: "content"},
    checkHighlighter: async function(testActor) {
      let {d} = await testActor.getHighlighterRegionPath("margin");
      ok(!d, "margin region is hidden");

      ({d} = await testActor.getHighlighterRegionPath("border"));
      ok(!d, "border region is hidden");

      ({d} = await testActor.getHighlighterRegionPath("padding"));
      ok(!d, "padding region is hidden");

      ({d} = await testActor.getHighlighterRegionPath("content"));
      ok(d, "content region is shown");
    }
  },
  {
    desc: "One region only can be shown (2)",
    options: {showOnly: "margin"},
    checkHighlighter: async function(testActor) {
      let {d} = await testActor.getHighlighterRegionPath("margin");
      ok(d, "margin region is shown");

      ({d} = await testActor.getHighlighterRegionPath("border"));
      ok(!d, "border region is hidden");

      ({d} = await testActor.getHighlighterRegionPath("padding"));
      ok(!d, "padding region is hidden");

      ({d} = await testActor.getHighlighterRegionPath("content"));
      ok(!d, "content region is hidden");
    }
  },
  {
    desc: "Guides can be drawn around a given region (1)",
    options: {region: "padding"},
    checkHighlighter: async function(testActor) {
      const topY1 = await testActor.getHighlighterNodeAttribute(
        "box-model-guide-top", "y1");
      const rightX1 = await testActor.getHighlighterNodeAttribute(
        "box-model-guide-right", "x1");
      const bottomY1 = await testActor.getHighlighterNodeAttribute(
        "box-model-guide-bottom", "y1");
      const leftX1 = await testActor.getHighlighterNodeAttribute(
        "box-model-guide-left", "x1");

      let {points} = await testActor.getHighlighterRegionPath("padding");
      points = points[0];

      is(topY1, points[0][1], "Top guide's y1 is correct");
      is(rightX1, points[1][0] - 1, "Right guide's x1 is correct");
      is(bottomY1, points[2][1] - 1, "Bottom guide's y1 is correct");
      is(leftX1, points[3][0], "Left guide's x1 is correct");
    }
  },
  {
    desc: "Guides can be drawn around a given region (2)",
    options: {region: "margin"},
    checkHighlighter: async function(testActor) {
      const topY1 = await testActor.getHighlighterNodeAttribute(
        "box-model-guide-top", "y1");
      const rightX1 = await testActor.getHighlighterNodeAttribute(
        "box-model-guide-right", "x1");
      const bottomY1 = await testActor.getHighlighterNodeAttribute(
        "box-model-guide-bottom", "y1");
      const leftX1 = await testActor.getHighlighterNodeAttribute(
        "box-model-guide-left", "x1");

      let {points} = await testActor.getHighlighterRegionPath("margin");
      points = points[0];

      is(topY1, points[0][1], "Top guide's y1 is correct");
      is(rightX1, points[1][0] - 1, "Right guide's x1 is correct");
      is(bottomY1, points[2][1] - 1, "Bottom guide's y1 is correct");
      is(leftX1, points[3][0], "Left guide's x1 is correct");
    }
  },
  {
    desc: "When showOnly is used, other regions can be faded",
    options: {showOnly: "margin", onlyRegionArea: true},
    checkHighlighter: async function(testActor) {
      for (const region of ["margin", "border", "padding", "content"]) {
        const {d} = await testActor.getHighlighterRegionPath(region);
        ok(d, "Region " + region + " is shown (it has a d attribute)");

        const faded = await testActor.getHighlighterNodeAttribute(
                          "box-model-" + region, "faded");
        if (region === "margin") {
          ok(!faded, "The margin region is not faded");
        } else {
          is(faded, "true", "Region " + region + " is faded");
        }
      }
    }
  },
  {
    desc: "When showOnly is used, other regions can be faded (2)",
    options: {showOnly: "padding", onlyRegionArea: true},
    checkHighlighter: async function(testActor) {
      for (const region of ["margin", "border", "padding", "content"]) {
        const {d} = await testActor.getHighlighterRegionPath(region);
        ok(d, "Region " + region + " is shown (it has a d attribute)");

        const faded = await testActor.getHighlighterNodeAttribute(
                          "box-model-" + region, "faded");
        if (region === "padding") {
          ok(!faded, "The padding region is not faded");
        } else {
          is(faded, "true", "Region " + region + " is faded");
        }
      }
    }
  }
];

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURI(TEST_URL));

  const divFront = await getNodeFront("div", inspector);

  for (const {desc, options, checkHighlighter} of TEST_DATA) {
    info("Running test: " + desc);

    info("Show the box-model highlighter with options " + options);
    await inspector.highlighter.showBoxModel(divFront, options);

    await checkHighlighter(testActor);

    info("Hide the box-model highlighter");
    await inspector.highlighter.hideBoxModel();
  }
});
