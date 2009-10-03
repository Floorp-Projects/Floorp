/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SMIL Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
