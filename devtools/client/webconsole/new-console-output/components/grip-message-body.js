/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// If this is being run from Mocha, then the browser loader hasn't set up
// define. We need to do that before loading Rep.
if (typeof define === "undefined") {
  require("amd-loader");
}

// React
const {
  createFactory,
  PropTypes
} = require("devtools/client/shared/vendor/react");

const VariablesViewLink = createFactory(require("devtools/client/webconsole/new-console-output/components/variables-view-link"));

const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const Rep = REPS.Rep;
const Grip = REPS.Grip;
const StringRep = REPS.StringRep.rep;

GripMessageBody.displayName = "GripMessageBody";

GripMessageBody.propTypes = {
  grip: PropTypes.oneOfType([
    PropTypes.string,
    PropTypes.number,
    PropTypes.object,
  ]).isRequired,
  serviceContainer: PropTypes.shape({
    createElement: PropTypes.func.isRequired,
  }),
  userProvidedStyle: PropTypes.string,
  useQuotes: PropTypes.bool,
  escapeWhitespace: PropTypes.bool,
};

GripMessageBody.defaultProps = {
  mode: MODE.LONG,
};

function GripMessageBody(props) {
  const {
    grip,
    userProvidedStyle,
    serviceContainer,
    useQuotes,
    escapeWhitespace
  } = props;

  let styleObject;
  if (userProvidedStyle && userProvidedStyle !== "") {
    styleObject = cleanupStyle(userProvidedStyle, serviceContainer.createElement);
  }

  let onDOMNodeMouseOver;
  let onDOMNodeMouseOut;
  if (serviceContainer) {
    onDOMNodeMouseOver = (object) => serviceContainer.highlightDomElement(object);
    onDOMNodeMouseOut = serviceContainer.unHighlightDomElement;
  }

  return (
    // @TODO once there is a longString rep, also turn off quotes for those.
    typeof grip === "string"
      ? StringRep({
        object: grip,
        useQuotes: useQuotes,
        escapeWhitespace: escapeWhitespace,
        mode: props.mode,
        style: styleObject
      })
      : Rep({
        object: grip,
        objectLink: VariablesViewLink,
        onDOMNodeMouseOver,
        onDOMNodeMouseOut,
        defaultRep: Grip,
        mode: props.mode,
      })
  );
}

function cleanupStyle(userProvidedStyle, createElement) {
  // Regular expression that matches the allowed CSS property names.
  const allowedStylesRegex = new RegExp(
    "^(?:-moz-)?(?:background|border|box|clear|color|cursor|display|float|font|line|" +
    "margin|padding|text|transition|outline|white-space|word|writing|" +
    "(?:min-|max-)?width|(?:min-|max-)?height)"
  );

  // Regular expression that matches the forbidden CSS property values.
  const forbiddenValuesRegexs = [
    // url(), -moz-element()
    /\b(?:url|(?:-moz-)?element)[\s('"]+/gi,

    // various URL protocols
    /['"(]*(?:chrome|resource|about|app|data|https?|ftp|file):+\/*/gi,
  ];

  // Use a dummy element to parse the style string.
  let dummy = createElement("div");
  dummy.style = userProvidedStyle;

  // Return a style object as expected by React DOM components, e.g.
  // {color: "red"}
  // without forbidden properties and values.
  return [...dummy.style]
    .filter(name => {
      return allowedStylesRegex.test(name)
        && !forbiddenValuesRegexs.some(regex => regex.test(dummy.style[name]));
    })
    .reduce((object, name) => {
      return Object.assign({
        [name]: dummy.style[name]
      }, object);
    }, {});
}

module.exports = GripMessageBody;
