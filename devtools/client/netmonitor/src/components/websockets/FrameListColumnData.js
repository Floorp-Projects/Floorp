/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getFramePayload } = require("../../utils/request-utils");
const { L10N } = require("../../utils/l10n");

/**
 * Renders the "Data" column of a WebSocket frame.
 */
class FrameListColumnData extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
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

  render() {
    const { type } = this.props.item;
    const typeLabel = L10N.getStr(`netmonitor.ws.type.${type}`);

    return dom.td(
      {
        className: "ws-frames-list-column ws-frames-list-payload",
        title: typeLabel + " " + this.state.payload,
      },
      dom.img({
        alt: typeLabel,
        className: `ws-frames-list-type-icon ws-frames-list-type-icon-${type}`,
        src: `chrome://devtools/content/netmonitor/src/assets/icons/arrow-up.svg`,
      }),
      " " + this.state.payload
    );
  }
}

module.exports = FrameListColumnData;
