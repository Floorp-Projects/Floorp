/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { WS_FRAMES_HEADERS } = require("../../constants");
const { L10N } = require("../../utils/l10n");

/**
 * Renders the frame list header.
 */
class FrameListHeader extends Component {
  constructor(props) {
    super(props);
  }

  /**
   * Render one column header from the table headers.
   */
  renderColumn({ name, width = "10%" }) {
    const label = L10N.getStr(`netmonitor.ws.toolbar.${name}`);

    return dom.th(
      {
        key: name,
        id: `ws-frames-list-${name}-header-box`,
        className: `ws-frames-list-column ws-frames-list-${name}`,
        scope: "col",
        style: { width },
      },
      dom.button(
        {
          id: `ws-frames-list-${name}-button`,
          className: `ws-frames-list-header-button`,
          title: label,
        },
        dom.div({ className: "button-text" }, label),
        dom.div({ className: "button-icon" })
      )
    );
  }

  /**
   * Render all columns in the table header
   */
  renderColumns() {
    return WS_FRAMES_HEADERS.map(header => this.renderColumn(header));
  }

  render() {
    return dom.thead(
      { className: "ws-frames-list-headers-group" },
      dom.tr(
        {
          className: "ws-frames-list-headers",
        },
        this.renderColumns()
      )
    );
  }
}

module.exports = FrameListHeader;
