/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const GripMessageBody = require("devtools/client/webconsole/new-console-output/components/GripMessageBody");
const ConsoleTable = createFactory(require("devtools/client/webconsole/new-console-output/components/ConsoleTable"));
const {isGroupType, l10n} = require("devtools/client/webconsole/new-console-output/utils/messages");

const Message = createFactory(require("devtools/client/webconsole/new-console-output/components/Message"));

ConsoleApiCall.displayName = "ConsoleApiCall";

ConsoleApiCall.propTypes = {
  dispatch: PropTypes.func.isRequired,
  message: PropTypes.object.isRequired,
  open: PropTypes.bool,
  serviceContainer: PropTypes.object.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
};

ConsoleApiCall.defaultProps = {
  open: false,
};

function ConsoleApiCall(props) {
  const {
    dispatch,
    message,
    open,
    tableData,
    serviceContainer,
    timestampsVisible,
    repeat,
  } = props;
  const {
    id: messageId,
    indent,
    source,
    type,
    level,
    stacktrace,
    frame,
    timeStamp,
    parameters,
    messageText,
    userProvidedStyles,
  } = message;

  let messageBody;
  const messageBodyConfig = {
    dispatch,
    messageId,
    parameters,
    userProvidedStyles,
    serviceContainer,
    type,
  };

  if (type === "trace") {
    messageBody = dom.span({className: "cm-variable"}, "console.trace()");
  } else if (type === "assert") {
    let reps = formatReps(messageBodyConfig);
    messageBody = dom.span({ className: "cm-variable" }, "Assertion failed: ", reps);
  } else if (type === "table") {
    // TODO: Chrome does not output anything, see if we want to keep this
    messageBody = dom.span({className: "cm-variable"}, "console.table()");
  } else if (parameters) {
    messageBody = formatReps(messageBodyConfig);
  } else {
    messageBody = messageText;
  }

  let attachment = null;
  if (type === "table") {
    attachment = ConsoleTable({
      dispatch,
      id: message.id,
      serviceContainer,
      parameters: message.parameters,
      tableData,
    });
  }

  let collapseTitle = null;
  if (isGroupType(type)) {
    collapseTitle = l10n.getStr("groupToggle");
  }

  const collapsible = isGroupType(type)
    || (type === "error" && Array.isArray(stacktrace));
  const topLevelClasses = ["cm-s-mozilla"];

  return Message({
    messageId,
    open,
    collapsible,
    collapseTitle,
    source,
    type,
    level,
    topLevelClasses,
    messageBody,
    repeat,
    frame,
    stacktrace,
    attachment,
    serviceContainer,
    dispatch,
    indent,
    timeStamp,
    timestampsVisible,
    parameters,
  });
}

function formatReps(options = {}) {
  const {
    dispatch,
    loadedObjectProperties,
    loadedObjectEntries,
    messageId,
    parameters,
    serviceContainer,
    userProvidedStyles,
    type,
  } = options;

  return (
    parameters
      // Get all the grips.
      .map((grip, key) => GripMessageBody({
        dispatch,
        messageId,
        grip,
        key,
        userProvidedStyle: userProvidedStyles ? userProvidedStyles[key] : null,
        serviceContainer,
        useQuotes: false,
        loadedObjectProperties,
        loadedObjectEntries,
        type,
      }))
      // Interleave spaces.
      .reduce((arr, v, i) => {
        // We need to interleave a space if we are not on the last element AND
        // if we are not between 2 messages with user provided style.
        const needSpace = i + 1 < parameters.length &&
          (!userProvidedStyles || !userProvidedStyles[i] || !userProvidedStyles[i + 1]);

        return needSpace
          ? arr.concat(v, dom.span({}, " "))
          : arr.concat(v);
      }, [])
  );
}

module.exports = ConsoleApiCall;

