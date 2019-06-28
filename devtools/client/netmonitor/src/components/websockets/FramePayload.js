/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getFramePayload } = require("../../utils/request-utils");

/**
 * Shows the full payload of a WebSocket frame.
 * The payload is unwrapped from the LongStringActor object.
 */
class FramePayload extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      selectedFrame: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      payload: "",
    };
  }

  componentDidMount() {
    const { selectedFrame, connector } = this.props;
    getFramePayload(selectedFrame.payload, connector.getLongString).then(
      payload => {
        this.setState({
          payload,
        });
      }
    );
  }

  componentWillReceiveProps(nextProps) {
    const { selectedFrame, connector } = nextProps;
    getFramePayload(selectedFrame.payload, connector.getLongString).then(
      payload => {
        this.setState({
          payload,
        });
      }
    );
  }

  render() {
    return div({ className: "ws-frame-payload" }, this.state.payload);
  }
}

module.exports = FramePayload;
