/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Message = createFactory(
  require("devtools/client/webconsole/components/Output/Message")
);
const GripMessageBody = require("devtools/client/webconsole/components/Output/GripMessageBody");
loader.lazyGetter(this, "REPS", function() {
  return require("devtools/client/shared/components/reps/reps").REPS;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});

PageError.displayName = "PageError";

PageError.propTypes = {
  message: PropTypes.object.isRequired,
  open: PropTypes.bool,
  timestampsVisible: PropTypes.bool.isRequired,
  serviceContainer: PropTypes.object,
  maybeScrollToBottom: PropTypes.func,
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
  } = message;

  const messageBody = [];

  const repsProps = {
    useQuotes: false,
    escapeWhitespace: false,
    openLink: serviceContainer.openLink,
  };

  if (hasException) {
    messageBody.push(
      "Uncaught ",
      GripMessageBody({
        dispatch,
        messageId,
        grip: parameters[0],
        serviceContainer,
        type,
        customFormat: true,
        ...repsProps,
      })
    );
  } else {
    messageBody.push(
      REPS.StringRep.rep({
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
