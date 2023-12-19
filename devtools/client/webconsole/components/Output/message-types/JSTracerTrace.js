/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const GripMessageBody = require("resource://devtools/client/webconsole/components/Output/GripMessageBody.js");

const Message = createFactory(
  require("resource://devtools/client/webconsole/components/Output/Message.js")
);

JSTracerTrace.displayName = "JSTracerTrace";

JSTracerTrace.propTypes = {
  dispatch: PropTypes.func.isRequired,
  message: PropTypes.object.isRequired,
  serviceContainer: PropTypes.object.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
  maybeScrollToBottom: PropTypes.func,
};

function JSTracerTrace(props) {
  const {
    dispatch,
    message,
    serviceContainer,
    timestampsVisible,
    repeat,
    maybeScrollToBottom,
    setExpanded,
  } = props;

  const {
    // List of common attributes for all tracer messages
    timeStamp,
    prefix,
    depth,
    source,

    // Attribute specific to DOM event
    eventName,

    // Attributes specific to function calls
    frame,
    implementation,
    displayName,
    parameters,
  } = message;

  // When we are logging a DOM event, we have the `eventName` defined.
  const messageBody = eventName
    ? [dom.span({ className: "jstracer-dom-event" }, eventName)]
    : [
        dom.span({ className: "jstracer-implementation" }, implementation),
        "⟶",
        dom.span({ className: "jstracer-display-name" }, displayName),
      ];

  // Arguments will only be passed on-demand
  if (parameters) {
    const messageBodyConfig = {
      dispatch,
      parameters,
      serviceContainer,
      type: "",
      maybeScrollToBottom,
      setExpanded,

      // Disable custom formatter for now in traces
      customFormat: false,
    };
    messageBody.push("(", ...formatReps(messageBodyConfig), ")");
  }

  if (prefix) {
    messageBody.unshift(
      dom.span(
        {
          className: "console-message-prefix",
        },
        `${prefix}`
      )
    );
  }

  const topLevelClasses = ["cm-s-mozilla"];

  return Message({
    collapsible: false,
    source,
    level: "jstracer",
    topLevelClasses,
    messageBody,
    repeat,
    frame,
    stacktrace: null,
    attachment: null,
    serviceContainer,
    dispatch,
    indent: depth,
    timeStamp,
    timestampsVisible,
    parameters,
    message,
    maybeScrollToBottom,
  });
}

function formatReps(options = {}) {
  const {
    dispatch,
    loadedObjectProperties,
    loadedObjectEntries,
    parameters,
    serviceContainer,
    type,
    maybeScrollToBottom,
    setExpanded,
    customFormat,
  } = options;

  const elements = [];
  const parametersLength = parameters.length;
  for (let i = 0; i < parametersLength; i++) {
    elements.push(
      GripMessageBody({
        dispatch,
        grip: parameters[i],
        key: i,
        serviceContainer,
        useQuotes: true,
        loadedObjectProperties,
        loadedObjectEntries,
        type,
        maybeScrollToBottom,
        setExpanded,
        customFormat,
      })
    );

    // We need to interleave a comma if we are not on the last element
    if (i !== parametersLength - 1) {
      elements.push(", ");
    }
  }

  return elements;
}

module.exports = JSTracerTrace;
