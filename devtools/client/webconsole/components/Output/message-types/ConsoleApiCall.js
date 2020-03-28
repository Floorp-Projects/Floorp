/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const GripMessageBody = require("devtools/client/webconsole/components/Output/GripMessageBody");
const ConsoleTable = createFactory(
  require("devtools/client/webconsole/components/Output/ConsoleTable")
);
const {
  isGroupType,
  l10n,
} = require("devtools/client/webconsole/utils/messages");

const Message = createFactory(
  require("devtools/client/webconsole/components/Output/Message")
);

ConsoleApiCall.displayName = "ConsoleApiCall";

ConsoleApiCall.propTypes = {
  dispatch: PropTypes.func.isRequired,
  message: PropTypes.object.isRequired,
  open: PropTypes.bool,
  serviceContainer: PropTypes.object.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
  maybeScrollToBottom: PropTypes.func,
};

ConsoleApiCall.defaultProps = {
  open: false,
};

function ConsoleApiCall(props) {
  const {
    dispatch,
    message,
    open,
    payload,
    serviceContainer,
    timestampsVisible,
    repeat,
    maybeScrollToBottom,
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
    prefix,
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
    maybeScrollToBottom,
  };

  if (type === "trace") {
    const traceParametersBody =
      Array.isArray(parameters) && parameters.length > 0
        ? [" "].concat(formatReps(messageBodyConfig))
        : [];

    messageBody = [
      dom.span({ className: "cm-variable" }, "console.trace()"),
      ...traceParametersBody,
    ];
  } else if (type === "assert") {
    const reps = formatReps(messageBodyConfig);
    messageBody = dom.span({}, "Assertion failed: ", reps);
  } else if (type === "table") {
    // TODO: Chrome does not output anything, see if we want to keep this
    messageBody = dom.span({ className: "cm-variable" }, "console.table()");
  } else if (parameters) {
    messageBody = formatReps(messageBodyConfig);
    if (prefix) {
      messageBody.unshift(
        dom.span(
          {
            className: "console-message-prefix",
          },
          `${prefix}: `
        )
      );
    }
  } else if (typeof messageText === "string") {
    messageBody = messageText;
  } else if (messageText) {
    messageBody = GripMessageBody({
      dispatch,
      messageId,
      grip: messageText,
      serviceContainer,
      useQuotes: false,
      transformEmptyString: true,
      type,
    });
  }

  let attachment = null;
  if (type === "table") {
    attachment = ConsoleTable({
      dispatch,
      id: message.id,
      serviceContainer,
      parameters: message.parameters,
      tableData: payload,
    });
  }

  let collapseTitle = null;
  if (isGroupType(type)) {
    collapseTitle = l10n.getStr("groupToggle");
  }

  const collapsible =
    isGroupType(type) || (type === "error" && Array.isArray(stacktrace));
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
    message,
    maybeScrollToBottom,
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
    maybeScrollToBottom,
  } = options;

  return (
    parameters
      // Get all the grips.
      .map((grip, key) =>
        GripMessageBody({
          dispatch,
          messageId,
          grip,
          key,
          userProvidedStyle: userProvidedStyles
            ? userProvidedStyles[key]
            : null,
          serviceContainer,
          useQuotes: false,
          loadedObjectProperties,
          loadedObjectEntries,
          type,
          maybeScrollToBottom,
        })
      )
      // Interleave spaces.
      .reduce((arr, v, i) => {
        // We need to interleave a space if we are not on the last element AND
        // if we are not between 2 messages with user provided style.
        const needSpace =
          i + 1 < parameters.length &&
          (!userProvidedStyles ||
            userProvidedStyles[i] === undefined ||
            userProvidedStyles[i + 1] === undefined);

        return needSpace ? arr.concat(v, " ") : arr.concat(v);
      }, [])
  );
}

module.exports = ConsoleApiCall;
