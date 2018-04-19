/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  MESSAGE_TYPE,
  JSTERM_COMMANDS,
} = require("../constants");
const { getObjectInspector } = require("devtools/client/webconsole/utils/object-inspector");
const actions = require("devtools/client/webconsole/actions/index");

const reps = require("devtools/client/shared/components/reps/reps");
const { MODE, ObjectInspectorUtils } = reps;

GripMessageBody.displayName = "GripMessageBody";

GripMessageBody.propTypes = {
  grip: PropTypes.oneOfType([
    PropTypes.string,
    PropTypes.number,
    PropTypes.object,
  ]).isRequired,
  serviceContainer: PropTypes.shape({
    createElement: PropTypes.func.isRequired,
    hudProxy: PropTypes.object.isRequired,
    onViewSourceInDebugger: PropTypes.func.isRequired,
  }),
  userProvidedStyle: PropTypes.string,
  useQuotes: PropTypes.bool,
  escapeWhitespace: PropTypes.bool,
  type: PropTypes.string,
  helperType: PropTypes.string,
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
    escapeWhitespace,
    mode = MODE.LONG,
    dispatch,
  } = props;

  let styleObject;
  if (userProvidedStyle && userProvidedStyle !== "") {
    styleObject = cleanupStyle(userProvidedStyle, serviceContainer.createElement);
  }

  const objectInspectorProps = {
    autoExpandDepth: shouldAutoExpandObjectInspector(props) ? 1 : 0,
    mode,
    // TODO: we disable focus since the tabbing trail is a bit weird in the output (e.g.
    // location links are not focused). Let's remove the property below when we found and
    // fixed the issue (See Bug 1456060).
    focusable: false,
    onCmdCtrlClick: (node, { depth, event, focused, expanded }) => {
      const value = ObjectInspectorUtils.node.getValue(node);
      if (value) {
        dispatch(actions.showObjectInSidebar(value));
      }
    }
  };

  if (typeof grip === "string" || (grip && grip.type === "longString")) {
    Object.assign(objectInspectorProps, {
      useQuotes,
      escapeWhitespace,
      style: styleObject
    });
  }

  return getObjectInspector(grip, serviceContainer, objectInspectorProps);
}

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

function cleanupStyle(userProvidedStyle, createElement) {
  // Use a dummy element to parse the style string.
  const dummy = createElement("div");
  dummy.style = userProvidedStyle;

  // Return a style object as expected by React DOM components, e.g.
  // {color: "red"}
  // without forbidden properties and values.
  return Array.from(dummy.style)
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

function shouldAutoExpandObjectInspector(props) {
  const {
    helperType,
    type,
  } = props;

  return (
    type === MESSAGE_TYPE.DIR
    || helperType === JSTERM_COMMANDS.INSPECT
  );
}

module.exports = GripMessageBody;
