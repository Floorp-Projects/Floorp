/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getFramePayload } = require("../../utils/request-utils");

/**
 * Renders the "Data" column of a WebSocket frame.
 */
class FrameListColumnData extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      index: PropTypes.number.isRequired,
      connector: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      payload: "",
    };
  }

  componentDidMount() {
    const { item, connector } = this.props;
    getFramePayload(item.payload, connector.getLongString).then(payload => {
      this.setState({
        payload,
      });
    });
  }

  componentWillReceiveProps(nextProps) {
    const { item, connector } = nextProps;
    getFramePayload(item.payload, connector.getLongString).then(payload => {
      this.setState({
        payload,
      });
    });
  }

  render() {
    const { index } = this.props;

    return dom.td(
      {
        key: index,
        className: "ws-frames-list-column ws-frames-list-payload",
        title: this.state.payload,
      },
      this.state.payload
    );
  }
}

module.exports = FrameListColumnData;
