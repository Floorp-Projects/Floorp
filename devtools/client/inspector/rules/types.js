/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * A CSS declaration.
 */
const declaration = exports.declaration = {
  // Array of the computed properties for a CSS declaration.
  computedProperties: PropTypes.arrayOf(PropTypes.shape({
    // The computed property name.
    name: PropTypes.string,
    // The computed priority (either "important" or an empty string).
    priority: PropTypes.string,
    // The computed property value.
    value: PropTypes.string,
  })),

  // An unique CSS declaration id.
  id: PropTypes.string,

  // Whether or not the declaration is enabled.
  isEnabled: PropTypes.bool,

  // Whether or not the declaration's property name is known.
  isKnownProperty: PropTypes.bool,

  // Whether or not the the declaration is overridden.
  isOverridden: PropTypes.bool,

  // The declaration's property name.
  name: PropTypes.string,

  // The declaration's priority (either "important" or an empty string).
  priority: PropTypes.string,

  // The declaration's property value.
  value: PropTypes.string,
};

/**
 * A CSS selector.
 */
const selector = exports.selector = {
  // Array of the selectors that match the selected element.
  matchedSelectors: PropTypes.arrayOf(PropTypes.string),
  // The CSS rule's selector text content.
  selectorText: PropTypes.string,
  // Array of the CSS rule's selectors.
  selectors: PropTypes.arrayOf(PropTypes.string),
};

/**
 * A CSS rule's stylesheet source.
 */
const sourceLink = exports.sourceLink = {
  // The CSS rule's column number within the stylesheet.
  column: PropTypes.number,
  // The CSS rule's line number within the stylesheet.
  line: PropTypes.number,
  // The media query text within a @media rule.
  // Note: Abstract this to support other at-rules in the future.
  mediaText: PropTypes.string,
  // The title used for the stylesheet source.
  title: PropTypes.string,
};

/**
 * A CSS Rule.
 */
exports.rule = {
  // Array of CSS declarations.
  declarations: PropTypes.arrayOf(PropTypes.shape(declaration)),

  // An unique CSS rule id.
  id: PropTypes.string,

  // An object containing information about the CSS rule's inheritance.
  inheritance: PropTypes.shape({
    // The NodeFront of the element this rule was inherited from.
    inherited: PropTypes.object,
    // A header label for where the element this rule was inherited from.
    inheritedSource: PropTypes.string,
  }),

  // Whether or not the rule does not match the current selected element.
  isUnmatched: PropTypes.bool,

  // Whether or not the rule is an user agent style.
  isUserAgentStyle: PropTypes.bool,

  // An object containing information about the CSS keyframes rules.
  keyframesRule: PropTypes.shape({
    // The actor ID of the keyframes rule.
    id: PropTypes.string,
    // The keyframes rule name.
    keyframesName: PropTypes.string,
  }),

  // An object containing information about the CSS rule's selector.
  selector: PropTypes.shape(selector),

  // An object containing information about the CSS rule's stylesheet source.
  sourceLink: PropTypes.shape(sourceLink),

  // The CSS rule type.
  type: PropTypes.number,
};
