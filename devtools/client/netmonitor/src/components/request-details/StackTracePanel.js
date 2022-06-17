/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  fetchNetworkUpdatePacket,
} = require("devtools/client/netmonitor/src/utils/request-utils");

const { div } = dom;

// Components
const StackTrace = createFactory(
  require("devtools/client/shared/components/StackTrace")
);

/**
 * This component represents a side panel responsible for
 * rendering stack-trace info for selected request.
 */
class StackTracePanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      request: PropTypes.object.isRequired,
      sourceMapURLService: PropTypes.object,
      openLink: PropTypes.func,
    };
  }

  /**
   * `componentDidMount` is called when opening the StackTracePanel
   * for the first time
   */
  componentDidMount() {
    const { request, connector } = this.props;
    if (!request.stacktrace) {
      fetchNetworkUpdatePacket(connector.requestData, request, ["stackTrace"]);
    }
  }

  /**
   * `componentWillReceiveProps` is the only method called when
   * switching between two requests while this panel is displayed.
   */
  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { request, connector } = nextProps;
    // Only try to fetch the stacktrace if we don't already have the stacktrace yet
    if (!request.stacktrace) {
      fetchNetworkUpdatePacket(connector.requestData, request, ["stackTrace"]);
    }
  }

  render() {
    const { connector, openLink, request, sourceMapURLService } = this.props;

    const { stacktrace } = request;

    return div(
      { className: "panel-container" },
      StackTrace({
        stacktrace: stacktrace || [],
        onViewSourceInDebugger: ({ url, line, column }) => {
          return connector.viewSourceInDebugger(url, line, column);
        },
        sourceMapURLService,
        openLink,
      })
    );
  }
}

module.exports = StackTracePanel;
