"use strict";

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactDom = require("devtools/client/shared/vendor/react-dom");

var _reactDom2 = _interopRequireDefault(_reactDom);

var _devtoolsEnvironment = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-environment"];

var _client = require("./client/index");

var _bootstrap = require("./utils/bootstrap");

var _sourceQueue = require("./utils/source-queue");

var _sourceQueue2 = _interopRequireDefault(_sourceQueue);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function unmountRoot() {
  const mount = document.querySelector("#mount .launchpad-root");

  _reactDom2.default.unmountComponentAtNode(mount);
}

if ((0, _devtoolsEnvironment.isFirefoxPanel)()) {
  module.exports = {
    bootstrap: ({
      threadClient,
      tabTarget,
      debuggerClient,
      sourceMaps,
      toolboxActions
    }) => {
      return (0, _client.onConnect)({
        tab: {
          clientType: "firefox"
        },
        tabConnection: {
          tabTarget,
          threadClient,
          debuggerClient
        }
      }, {
        services: {
          sourceMaps
        },
        toolboxActions
      });
    },
    destroy: () => {
      unmountRoot();

      _sourceQueue2.default.clear();

      (0, _bootstrap.teardownWorkers)();
    }
  };
} else {
  const {
    bootstrap,
    L10N
  } = require("devtools/shared/flags");

  window.L10N = L10N; // $FlowIgnore:

  window.L10N.setBundle(require("devtools/shared/flags"));
  bootstrap(_react2.default, _reactDom2.default).then(connection => {
    (0, _client.onConnect)(connection, {
      services: {
        sourceMaps: require("devtools/client/shared/source-map/index.js")
      },
      toolboxActions: {}
    });
  });
}