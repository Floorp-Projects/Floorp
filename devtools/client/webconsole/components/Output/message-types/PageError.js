/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const Message = createFactory(
  require("resource://devtools/client/webconsole/components/Output/Message.js")
);
const GripMessageBody = require("resource://devtools/client/webconsole/components/Output/GripMessageBody.js");
loader.lazyGetter(this, "REPS", function () {
  return require("resource://devtools/client/shared/components/reps/index.js")
    .REPS;
});
loader.lazyGetter(this, "MODE", function () {
  return require("resource://devtools/client/shared/components/reps/index.js")
    .MODE;
});

PageError.displayName = "PageError";

PageError.propTypes = {
  message: PropTypes.object.isRequired,
  open: PropTypes.bool,
  timestampsVisible: PropTypes.bool.isRequired,
  serviceContainer: PropTypes.object,
  maybeScrollToBottom: PropTypes.func,
  setExpanded: PropTypes.func,
  inWarningGroup: PropTypes.bool.isRequired,
};

PageError.defaultProps = {
  open: false,
};

function PageError(props) {
  const {
    dispatch,
    message,
    open,
    repeat,
    serviceContainer,
    timestampsVisible,
    maybeScrollToBottom,
    setExpanded,
    inWarningGroup,
  } = props;
  const {
    id: messageId,
    source,
    type,
    level,
    messageText,
    stacktrace,
    frame,
    exceptionDocURL,
    timeStamp,
    notes,
    parameters,
    hasException,
    isPromiseRejection,
  } = message;

  const messageBody = [];

  const repsProps = {
    useQuotes: false,
    escapeWhitespace: false,
    openLink: serviceContainer.openLink,
  };

  if (hasException) {
    const prefix = `Uncaught${isPromiseRejection ? " (in promise)" : ""} `;
    messageBody.push(
      prefix,
      GripMessageBody({
        key: "body",
        dispatch,
        messageId,
        grip: parameters[0],
        serviceContainer,
        type,
        customFormat: true,
        maybeScrollToBottom,
        setExpanded,
        ...repsProps,
      })
    );
  } else {
    messageBody.push(
      REPS.StringRep.rep({
        key: "bodytext",
        object: messageText,
        mode: MODE.LONG,
        ...repsProps,
      })
    );
  }

  return Message({
    dispatch,
    messageId,
    open,
    collapsible: Array.isArray(stacktrace),
    source,
    type,
    level,
    topLevelClasses: [],
    indent: message.indent,
    inWarningGroup,
    messageBody,
    repeat,
    frame,
    stacktrace,
    serviceContainer,
    exceptionDocURL,
    timeStamp,
    notes,
    timestampsVisible,
    maybeScrollToBottom,
    message,
  });
}

module.exports = PageError;
