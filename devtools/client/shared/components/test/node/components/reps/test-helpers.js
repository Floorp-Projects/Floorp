/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");

const {
  lengthBubble,
} = require("devtools/client/shared/components/reps/shared/grip-length-bubble");
const {
  maxLengthMap: arrayLikeMaxLengthMap,
  getLength: getArrayLikeLength,
} = require("devtools/client/shared/components/reps/reps/grip-array");
const {
  maxLengthMap: mapMaxLengths,
  getLength: getMapLength,
} = require("devtools/client/shared/components/reps/reps/grip-map");
const {
  getGripPreviewItems,
} = require("devtools/client/shared/components/reps/reps/rep-utils");
const nodeConstants = require("devtools/client/shared/components/reps/shared/dom-node-constants");

/**
 * Get an array of all the items from the grip in parameter (including the grip
 * itself) which can be selected in the inspector.
 *
 * @param {Object} Grip
 * @return {Array} Flat array of the grips which can be selected in the
 *                 inspector
 */
function getSelectableInInspectorGrips(grip) {
  const grips = new Set(getFlattenedGrips([grip]));
  return [...grips].filter(isGripSelectableInInspector);
}

/**
 * Indicate if a Grip can be selected in the inspector,
 * i.e. if it represents a node element.
 *
 * @param {Object} Grip
 * @return {Boolean}
 */
function isGripSelectableInInspector(grip) {
  return (
    grip &&
    typeof grip === "object" &&
    grip.preview &&
    [nodeConstants.TEXT_NODE, nodeConstants.ELEMENT_NODE].includes(
      grip.preview.nodeType
    )
  );
}

/**
 * Get a flat array of all the grips and their preview items.
 *
 * @param {Array} Grips
 * @return {Array} Flat array of the grips and their preview items
 */
function getFlattenedGrips(grips) {
  return grips.reduce((res, grip) => {
    const previewItems = getGripPreviewItems(grip);
    const flatPreviewItems = previewItems.length
      ? getFlattenedGrips(previewItems)
      : [];

    return [...res, grip, ...flatPreviewItems];
  }, []);
}

function expectActorAttribute(wrapper, expectedValue) {
  const actorIdAttribute = "data-link-actor-id";
  const attrElement = wrapper.find(`[${actorIdAttribute}]`);
  expect(attrElement.exists()).toBeTruthy();
  expect(attrElement.first().prop("data-link-actor-id")).toBe(expectedValue);
}

function getGripLengthBubbleText(object, props) {
  const component = lengthBubble({
    object,
    maxLengthMap: arrayLikeMaxLengthMap,
    getLength: getArrayLikeLength,
    ...props,
  });

  return component ? shallow(component).text() : "";
}

function getMapLengthBubbleText(object, props) {
  return getGripLengthBubbleText(object, {
    maxLengthMap: mapMaxLengths,
    getLength: getMapLength,
    showZeroLength: true,
    ...props,
  });
}

function createGripMapEntry(key, value) {
  return {
    type: "mapEntry",
    preview: {
      key,
      value,
    },
  };
}

module.exports = {
  createGripMapEntry,
  expectActorAttribute,
  getSelectableInInspectorGrips,
  getGripLengthBubbleText,
  getMapLengthBubbleText,
};
