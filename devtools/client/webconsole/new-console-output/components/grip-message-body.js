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
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { ObjectClient } = require("devtools/shared/client/main");

const actions = require("devtools/client/webconsole/new-console-output/actions/messages");
const reps = require("devtools/client/shared/components/reps/reps");
const { REPS, MODE } = reps;
const ObjectInspector = createFactory(reps.ObjectInspector);
const { Grip } = REPS;

GripMessageBody.displayName = "GripMessageBody";

GripMessageBody.propTypes = {
  grip: PropTypes.oneOfType([
    PropTypes.string,
    PropTypes.number,
    PropTypes.object,
  ]).isRequired,
  serviceContainer: PropTypes.shape({
    createElement: PropTypes.func.isRequired,
    hudProxyClient: PropTypes.object.isRequired,
  }),
  userProvidedStyle: PropTypes.string,
  useQuotes: PropTypes.bool,
  escapeWhitespace: PropTypes.bool,
  loadedObjectProperties: PropTypes.object,
};

GripMessageBody.defaultProps = {
  mode: MODE.LONG,
};

function GripMessageBody(props) {
  const {
    dispatch,
    messageId,
    grip,
    userProvidedStyle,
    serviceContainer,
    useQuotes,
    escapeWhitespace,
    mode = MODE.LONG,
    loadedObjectProperties,
  } = props;

  let styleObject;
  if (userProvidedStyle && userProvidedStyle !== "") {
    styleObject = cleanupStyle(userProvidedStyle, serviceContainer.createElement);
  }

  let onDOMNodeMouseOver;
  let onDOMNodeMouseOut;
  let onInspectIconClick;
  if (serviceContainer) {
    onDOMNodeMouseOver = serviceContainer.highlightDomElement
      ? (object) => serviceContainer.highlightDomElement(object)
      : null;
    onDOMNodeMouseOut = serviceContainer.unHighlightDomElement;
    onInspectIconClick = serviceContainer.openNodeInInspector
      ? (object, e) => {
        // Stop the event propagation so we don't trigger ObjectInspector expand/collapse.
        e.stopPropagation();
        serviceContainer.openNodeInInspector(object);
      }
      : null;
  }

  const objectInspectorProps = {
    autoExpandDepth: 0,
    mode,
    // TODO: we disable focus since it's not currently working well in ObjectInspector.
    // Let's remove the property below when problem are fixed in OI.
    disabledFocus: true,
    roots: [{
      path: grip.actor || JSON.stringify(grip),
      contents: {
        value: grip
      }
    }],
    getObjectProperties: actor => loadedObjectProperties && loadedObjectProperties[actor],
    loadObjectProperties: object => {
      const client = new ObjectClient(serviceContainer.hudProxyClient, object);
      dispatch(actions.messageObjectPropertiesLoad(messageId, client, object));
    },
  };

  if (typeof grip === "string" || grip.type === "longString") {
    Object.assign(objectInspectorProps, {
      useQuotes,
      escapeWhitespace,
      style: styleObject
    });
  } else {
    Object.assign(objectInspectorProps, {
      onDOMNodeMouseOver,
      onDOMNodeMouseOut,
      onInspectIconClick,
      defaultRep: Grip,
    });
  }

  return ObjectInspector(objectInspectorProps);
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
