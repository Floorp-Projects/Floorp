/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check animated properties's graph.
// The graph constructs from SVG, also uses path (for shape), linearGradient,
// stop (for color) element and so on.
// We test followings.
// 1. class name - which represents the animation type.
// 2. coordinates of the path - x is time, y is graph y value which should be 0 - 1.
//    The path of animation types 'color', 'coord', 'opacity' or 'discrete' are created by
//    createPathSegments. Other types are created by createKeyframesPathSegments.
// 3. color - animation type 'color' has linearGradient element.

requestLongerTimeout(5);

const TEST_CASES = [
  {
    "background-color": {
      expectedClass: "color",
      expectedValues: [
        { x: 0, y: 1, color: "rgb(255, 0, 0)" },
        { x: 1000, y: 1, color: "rgb(0, 255, 0)" }
      ]
    },
    "font-size": {
      expectedClass: "length",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 1000, y: 1 },
      ]
    },
    "margin-left": {
      expectedClass: "coord",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 1000, y: 1 },
      ]
    },
    "opacity": {
      expectedClass: "opacity",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 1000, y: 1 },
      ]
    },
    "text-align": {
      expectedClass: "discrete",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 499.999, y: 0 },
        { x: 500, y: 1 },
        { x: 1000, y: 1 },
      ]
    },
    "transform": {
      expectedClass: "transform",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 1000, y: 1 },
      ]
    }
  },

  {
    "background-color": {
      expectedClass: "color",
      expectedValues: [
        { x: 0, y: 1, color: "rgb(0, 255, 0)" },
        { x: 1000, y: 1, color: "rgb(255, 0, 0)" }
      ]
    },
    "font-size": {
      expectedClass: "length",
      expectedValues: [
        { x: 0, y: 1 },
        { x: 1000, y: 0 },
      ]
    },
    "margin-left": {
      expectedClass: "coord",
      expectedValues: [
        { x: 0, y: 1 },
        { x: 1000, y: 0 },
      ]
    },
    "opacity": {
      expectedClass: "opacity",
      expectedValues: [
        { x: 0, y: 1 },
        { x: 1000, y: 0 },
      ]
    },
    "text-align": {
      expectedClass: "discrete",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 499.999, y: 0 },
        { x: 500, y: 1 },
        { x: 1000, y: 1 },
      ]
    },
    "transform": {
      expectedClass: "transform",
      expectedValues: [
        { x: 0, y: 1 },
        { x: 1000, y: 0 },
      ]
    }
  },

  {
    "background-color": {
      expectedClass: "color",
      expectedValues: [
        { x: 0, y: 1, color: "rgb(255, 0, 0)" },
        { x: 500, y: 1, color: "rgb(0, 0, 255)" },
        { x: 1000, y: 1, color: "rgb(0, 255, 0)" }
      ]
    },
    "font-size": {
      expectedClass: "length",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 500, y: 1 },
        { x: 1000, y: 0 },
      ]
    },
    "margin-left": {
      expectedClass: "coord",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 500, y: 1 },
        { x: 1000, y: 0 },
      ]
    },
    "opacity": {
      expectedClass: "opacity",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 500, y: 1 },
        { x: 1000, y: 0 },
      ]
    },
    "text-align": {
      expectedClass: "discrete",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 249.999, y: 0 },
        { x: 250, y: 1 },
        { x: 749.999, y: 1 },
        { x: 750, y: 0 },
        { x: 1000, y: 0 },
      ]
    },
    "transform": {
      expectedClass: "transform",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 500, y: 1 },
        { x: 1000, y: 0 },
      ]
    }
  },
  {
    "background-color": {
      expectedClass: "color",
      expectedValues: [
        { x: 0, y: 1, color: "rgb(255, 0, 0)" },
        { x: 499.999, y: 1, color: "rgb(255, 0, 0)" },
        { x: 500, y: 1, color: "rgb(128, 128, 0)" },
        { x: 999.999, y: 1, color: "rgb(128, 128, 0)" },
        { x: 1000, y: 1, color: "rgb(0, 255, 0)" }
      ]
    },
    "font-size": {
      expectedClass: "length",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 500, y: 0 },
        { x: 500, y: 0.5 },
        { x: 1000, y: 0.5 },
        { x: 1000, y: 1 },
      ]
    },
    "margin-left": {
      expectedClass: "coord",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 499.999, y: 0 },
        { x: 500, y: 0.5 },
        { x: 999.999, y: 0.5 },
        { x: 1000, y: 1 },
      ]
    },
    "opacity": {
      expectedClass: "opacity",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 499.999, y: 0 },
        { x: 500, y: 0.5 },
        { x: 999.999, y: 0.5 },
        { x: 1000, y: 1 },
      ]
    },
    "text-align": {
      expectedClass: "discrete",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 499.999, y: 0 },
        { x: 500, y: 1 },
        { x: 1000, y: 1 },
      ]
    },
    "transform": {
      expectedClass: "transform",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 500, y: 0 },
        { x: 500, y: 0.5 },
        { x: 1000, y: 0.5 },
        { x: 1000, y: 1 },
      ]
    }
  },
  {
    "opacity": {
      expectedClass: "opacity",
      expectedValues: [
        { x: 0, y: 0 },
        { x: 500, y: 0.5 },
        { x: 1000, y: 1 },
      ]
    }
  }
];

add_task(function* () {
  yield addTab(URL_ROOT + "doc_multiple_property_types.html");
  const {panel} = yield openAnimationInspector();
  const timelineComponent = panel.animationsTimelineComponent;
  const detailEl = timelineComponent.details.containerEl;

  for (let i = 0; i < TEST_CASES.length; i++) {
    info(`Click to select the animation[${ i }]`);
    yield clickOnAnimation(panel, i);
    const timeBlock = timelineComponent.timeBlocks[0];
    const state = timeBlock.animation.state;
    const properties = TEST_CASES[i];
    for (let property in properties) {
      const testcase = properties[property];
      info(`Test path of ${ property }`);
      const className = testcase.expectedClass;
      const pathEl = detailEl.querySelector(`path.${ className }`);
      ok(pathEl, `Path element with class '${ className }' should exis`);
      checkPathSegments(pathEl, state, testcase.expectedValues);
    }
  }
});

function checkPathSegments(pathEl, { duration }, expectedValues) {
  const pathSegList = pathEl.pathSegList;

  const firstPathSeg = pathSegList.getItem(0);
  is(firstPathSeg.x, 0, "The x of first segment should be 0");
  is(firstPathSeg.y, 0, "The y of first segment should be 0");

  expectedValues.forEach(expectedValue => {
    ok(hasSegment(pathSegList, expectedValue.x, expectedValue.y),
       `The path segment of x ${ expectedValue.x }, y ${ expectedValue.y } should exist`);

    if (expectedValue.color) {
      checkColor(pathEl.closest("svg"), expectedValue.x / duration, expectedValue.color);
    }
  });

  const closePathSeg = pathSegList.getItem(pathSegList.numberOfItems - 1);
  is(closePathSeg.pathSegType, closePathSeg.PATHSEG_CLOSEPATH,
     "The actual last segment should be close path");
}

function checkColor(svgEl, offset, color) {
  const stopEl = findStopElement(svgEl, offset);
  ok(stopEl, `stop element at offset ${ offset } should exist`);
  is(stopEl.getAttribute("stop-color"), color,
     `stop-color of stop element at offset ${ offset } should be ${ color }`);
}

function hasSegment(pathSegList, x, y) {
  for (let i = 1; i < pathSegList.numberOfItems - 1; i++) {
    const pathSeg = pathSegList.getItem(i);
    if (parseFloat(pathSeg.x.toFixed(3)) === x &&
        parseFloat(pathSeg.y.toFixed(6)) === y) {
      return true;
    }
  }
  return false;
}

function findStopElement(svgEl, offset) {
  return [...svgEl.querySelectorAll("stop")].find(stopEl => {
    return stopEl.getAttribute("offset") == offset;
  });
}
