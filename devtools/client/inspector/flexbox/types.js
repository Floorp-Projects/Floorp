/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * A flex item computed style properties.
 */
const flexItemProperties = exports.flexItemProperties = {

  // The computed value of flex-basis.
  "flex-basis": PropTypes.string,

  // The computed value of flex-grow.
  "flex-grow": PropTypes.string,

  // The computed value of min-height.
  "min-height": PropTypes.string,

  // The computed value of min-width.
  "min-width": PropTypes.string,

  // The computed value of max-height.
  "max-height": PropTypes.string,

  // The computed value of max-width.
  "max-width": PropTypes.string,

  // The computed height of the flex item element.
  "height": PropTypes.string,

  // The computed width of the flex item element.
  "width": PropTypes.string,

};

/**
 * A flex item sizing data.
 */
const flexItemSizing = exports.flexItemSizing = {

  // The max size of the flex item in the cross axis.
  crossMaxSize: PropTypes.number,

  // The min size of the flex item in the cross axis.
  crossMinSize: PropTypes.number,

  // The size of the flex item in the main axis before the flex sizing algorithm is
  // applied. This is also called the "flex base size" in the spec.
  mainBaseSize: PropTypes.number,

  // The value that the flex sizing algorithm "wants" to use to stretch or shrink the
  // item, before clamping to the item's main min and max sizes.
  mainDeltaSize: PropTypes.number,

  // The max size of the flex item in the main axis.
  mainMaxSize: PropTypes.number,

  // The min size of the flex item in the maxin axis.
  mainMinSize: PropTypes.number,

};

/**
 * A flex item data.
 */
const flexItem = exports.flexItem = {

  // The actor ID of the flex item.
  actorID: PropTypes.string,

  // The computed style properties of the flex item.
  computedStyle: PropTypes.object,

  // The flex item sizing data.
  flexItemSizing: PropTypes.shape(flexItemSizing),

  // The NodeFront of the flex item.
  nodeFront: PropTypes.object,

  // The authored style properties of the flex item.
  properties: PropTypes.shape(flexItemProperties),

};

/**
 * A flex container computed style properties.
 */
const flexContainerProperties = exports.flexContainerProperties = {

  // The computed value of align-content.
  "align-content": PropTypes.string,

  // The computed value of align-items.
  "align-items": PropTypes.string,

  // The computed value of flex-direction.
  "flex-direction": PropTypes.string,

  // The computed value of flex-wrap.
  "flex-wrap": PropTypes.string,

  // The computed value of justify-content.
  "justify-content": PropTypes.string,

};

/**
 * A flex container data.
 */
const flexContainer = exports.flexContainer = {

  // The actor ID of the flex container.
  actorID: PropTypes.string,

  // An array of flex items belonging to the flex container.
  flexItems: PropTypes.arrayOf(PropTypes.shape(flexItem)),

  // Whether or not the flex container data represents the selected flex container
  // or the parent flex container. This is true if the flex container data represents
  // the parent flex container.
  isFlexItemContainer: PropTypes.bool,

  // The NodeFront actor ID of the flex item to display in the flex item sizing
  // properties.
  flexItemShown: PropTypes.string,

  // The NodeFront of the flex container.
  nodeFront: PropTypes.object,

  // The computed style properties of the flex container.
  properties: PropTypes.shape(flexContainerProperties),

};

/**
 * The Flexbox UI state.
 */
exports.flexbox = {

  // The color of the flexbox highlighter overlay.
  color: PropTypes.string,

  // The selected flex container.
  flexContainer: PropTypes.shape(flexContainer),

  // The selected flex container can also be a flex item. This object contains the
  // parent flex container properties.
  flexItemContainer: PropTypes.shape(flexContainer),

  // Whether or not the flexbox highlighter is highlighting the flex container.
  highlighted: PropTypes.bool,

};
