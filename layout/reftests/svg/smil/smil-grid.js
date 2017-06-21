/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Javascript library for dynamically generating a simple SVG/SMIL reftest
 * with several copies of the same animation, each seeked to a different time.
 */

// Global variables
const START_TIMES = [ "4.0s",  "3.0s",  "2.7s",
                      "2.25s", "2.01s", "1.5s",
                      "1.4s",  "1.0s",  "0.5s" ];

const X_POSNS = [ "20px",  "70px",  "120px",
                  "20px",  "70px",  "120px",
                  "20px",  "70px",  "120px" ];

const Y_POSNS = [ "20px",  "20px",  "20px",
                  "70px",  "70px",  "70px",
                 "120px", "120px", "120px"  ];

const DURATION = "2s";
const SNAPSHOT_TIME ="3";
const SVGNS = "http://www.w3.org/2000/svg";

// Convenience wrapper using testAnimatedGrid to make 15pt-by-15pt rects
function testAnimatedRectGrid(animationTagName, animationAttrHashList) {
  var targetTagName = "rect";
  var targetAttrHash = {"width"  : "15px",
                        "height" : "15px" };
  testAnimatedGrid(targetTagName,    targetAttrHash,
                   animationTagName, animationAttrHashList);
}

// Convenience wrapper using testAnimatedGrid to make grid of text
function testAnimatedTextGrid(animationTagName, animationAttrHashList) {
  var targetTagName = "text";
  var targetAttrHash = { };
  testAnimatedGrid(targetTagName,    targetAttrHash,
                   animationTagName, animationAttrHashList);
}

// Generates a visual grid of elements of type "targetTagName", with the
// attribute values given in targetAttrHash.  Each generated element has
// exactly one child -- an animation element of type "animationTagName", with
// the attribute values given in animationAttrHash.
function testAnimatedGrid(targetTagName,    targetAttrHash,
                          animationTagName, animationAttrHashList) {
    // SANITY CHECK
  const numElementsToMake = START_TIMES.length;
  if (X_POSNS.length != numElementsToMake ||
      Y_POSNS.length != numElementsToMake) {
    return;
  }

  for (var i = 0; i < animationAttrHashList.length; i++) {
    var animationAttrHash = animationAttrHashList[i];
    // Default to fill="freeze" so we can test the final value of the animation
    if (!animationAttrHash["fill"]) {
      animationAttrHash["fill"] = "freeze";
    }
  }

  // Build the grid!
  var svg = document.documentElement;
  for (var i = 0; i < numElementsToMake; i++) {
    // Build target & animation elements
    var targetElem = buildElement(targetTagName, targetAttrHash);
    for (var j = 0; j < animationAttrHashList.length; j++) {
      var animationAttrHash = animationAttrHashList[j];
      var animElem = buildElement(animationTagName, animationAttrHash);

      // Customize them using global constant values
      targetElem.setAttribute("x", X_POSNS[i]);
      targetElem.setAttribute("y", Y_POSNS[i]);
      animElem.setAttribute("begin", START_TIMES[i]);
      animElem.setAttribute("dur", DURATION);

      // Append to target
      targetElem.appendChild(animElem);
    }
    // Insert target into DOM
    svg.appendChild(targetElem);
  }

  // Take snapshot
  setTimeAndSnapshot(SNAPSHOT_TIME, true);
}

// Generates a visual grid of elements of type |graphicElemTagName|, with the
// attribute values given in |graphicElemAttrHash|. This is a variation of the
// above function. We use <defs> to include the reference elements because
// some animatable properties are only applicable to some specific elements
// (e.g. feFlood, stop), so then we apply an animation element of type
// |animationTagName|, with the attribute values given in |animationAttrHash|,
// to those specific elements. |defTagNameList| is an array of tag names.
// We will create elements hierarchically according to this array. The first tag
// in |defTagNameList| is the outer-most one in <defs>, and the last tag is the
// inner-most one and it is the target to which the animation element will be
// applied. We visualize the effect of our animation by referencing each
// animated subtree from some graphical element that we generate. The
// |graphicElemIdValueProperty| parameter provides the name of the CSS property
// that we should use to hook up this reference.
//
// e.g. if a caller passes a defTagNameList of [ "linearGradient", "stop" ],
//      this function will generate the following subtree:
// <defs>
//   <linearGradient id="elem0">
//     <stop>
//       <animate ..../>
//     </stop>
//   </linearGradient>
//   <linearGradient id="elem1">
//     <stop>
//       <animate ..../>
//     </stop>
//   </linearGradient>
//
//   <!--- more similar linearGradients here, up to START_TIMES.length -->
// </defs>
function testAnimatedGridWithDefs(graphicElemTagName,
                                  graphicElemAttrHash,
                                  graphicElemIdValuedProperty,
                                  defTagNameList,
                                  animationTagName,
                                  animationAttrHashList) {
  // SANITY CHECK
  const numElementsToMake = START_TIMES.length;
  if (X_POSNS.length != numElementsToMake ||
      Y_POSNS.length != numElementsToMake) {
    return;
  }

  if (defTagNameList.length == 0) {
    return;
  }

  for (var i = 0; i < animationAttrHashList.length; i++) {
    var animationAttrHash = animationAttrHashList[i];
    // Default to fill="freeze" so we can test the final value of the animation
    if (!animationAttrHash["fill"]) {
      animationAttrHash["fill"] = "freeze";
    }
  }

  var svg = document.documentElement;

  // Build defs element.
  var defs = buildElement('defs');
  for (var i = 0; i < numElementsToMake; i++) {
    // This will track the innermost element in our subtree:
    var innerElement = defs;

    for (var defIdx = 0; defIdx < defTagNameList.length; ++defIdx) {
      // Set an ID on the first level of nesting (on child of defs):
      var attrs = defIdx == 0 ? { "id": "elem" + i } : {};

      var newElem = buildElement(defTagNameList[defIdx], attrs);
      innerElement.appendChild(newElem);
      innerElement = newElem;
    }

    for (var j = 0; j < animationAttrHashList.length; ++j) {
      var animationAttrHash = animationAttrHashList[j];
      var animElem = buildElement(animationTagName, animationAttrHash);
      animElem.setAttribute("begin", START_TIMES[i]);
      animElem.setAttribute("dur", DURATION);
      innerElement.appendChild(animElem);
    }
  }
  svg.appendChild(defs);

  // Build the grid!
  for (var i = 0; i < numElementsToMake; ++i) {
    var graphicElem = buildElement(graphicElemTagName, graphicElemAttrHash);
    graphicElem.setAttribute("x", X_POSNS[i]);
    graphicElem.setAttribute("y", Y_POSNS[i]);
    graphicElem.setAttribute("style", graphicElemIdValuedProperty +
                                      ":url(#elem" + i + ")");
    svg.appendChild(graphicElem);
  }

  // Take snapshot
  setTimeAndSnapshot(SNAPSHOT_TIME, true);
}

function buildElement(tagName, attrHash) {
  var elem = document.createElementNS(SVGNS, tagName);
  for (var attrName in attrHash) {
    var attrValue = attrHash[attrName];
    elem.setAttribute(attrName, attrValue);
  }
  // If we're creating a text node, populate it with some text.
  if (tagName == "text") {
    elem.appendChild(document.createTextNode("abc"));
  }
  return elem;
}
