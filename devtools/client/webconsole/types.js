/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE,
  MESSAGE_LEVEL,
} = require("devtools/client/webconsole/constants");

exports.ConsoleCommand = function(props) {
  return Object.assign(
    {
      id: null,
      allowRepeating: false,
      messageText: null,
      source: MESSAGE_SOURCE.JAVASCRIPT,
      type: MESSAGE_TYPE.COMMAND,
      level: MESSAGE_LEVEL.LOG,
      groupId: null,
      indent: 0,
      private: false,
      timeStamp: null,
    },
    props
  );
};

exports.ConsoleMessage = function(props) {
  return Object.assign(
    {
      id: null,
      innerWindowID: null,
      targetFront: null,
      allowRepeating: true,
      source: null,
      timeStamp: null,
      type: null,
      helperType: null,
      level: null,
      category: null,
      messageText: null,
      parameters: null,
      repeatId: null,
      stacktrace: null,
      frame: null,
      groupId: null,
      errorMessageName: null,
      exceptionDocURL: null,
      cssSelectors: "",
      userProvidedStyles: null,
      notes: null,
      indent: 0,
      prefix: "",
      private: false,
      chromeContext: false,
      hasException: false,
      isPromiseRejection: false,
    },
    props
  );
};

exports.NetworkEventMessage = function(props) {
  return Object.assign(
    {
      id: null,
      actor: null,
      targetFront: null,
      level: MESSAGE_LEVEL.LOG,
      isXHR: false,
      request: null,
      response: null,
      source: MESSAGE_SOURCE.NETWORK,
      type: MESSAGE_TYPE.LOG,
      groupId: null,
      timeStamp: null,
      totalTime: null,
      indent: 0,
      updates: null,
      securityState: null,
      securityInfo: null,
      requestHeadersFromUploadStream: null,
      private: false,
      blockedReason: null,
    },
    props
  );
};
