/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  WS_FRAMES_HEADERS,
} = require("devtools/client/netmonitor/src/constants.js");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const Actions = require("devtools/client/netmonitor/src/actions/index.js");

// Components
const FrameListHeaderContextMenu = require("devtools/client/netmonitor/src/components/websockets/FrameListHeaderContextMenu.js");

/**
 * Renders the frame list header.
 */
class FrameListHeader extends Component {
  static get propTypes() {
    return {
      columns: PropTypes.object.isRequired,
      toggleColumn: PropTypes.func.isRequired,
      resetColumns: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onContextMenu = this.onContextMenu.bind(this);
  }

  onContextMenu(evt) {
    evt.preventDefault();
    const { resetColumns, toggleColumn, columns } = this.props;

    this.contextMenu = new FrameListHeaderContextMenu({
      toggleColumn,
      resetColumns,
    });
    this.contextMenu.open(evt, columns);
  }

  /**
   * Helper method to get visibleColumns.
   */
  getVisibleColumns() {
    const { columns } = this.props;
    return WS_FRAMES_HEADERS.filter(header => columns[header.name]);
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
   * Render all columns in the table header.
   */
  renderColumns() {
    const visibleColumns = this.getVisibleColumns();
    return visibleColumns.map(header => this.renderColumn(header));
  }

  render() {
    return dom.thead(
      { className: "ws-frames-list-headers-group" },
      dom.tr(
        {
          className: "ws-frames-list-headers",
          onContextMenu: this.onContextMenu,
        },
        this.renderColumns()
      )
    );
  }
}

module.exports = connect(
  state => ({
    columns: state.webSockets.columns,
  }),
  dispatch => ({
    toggleColumn: column => dispatch(Actions.toggleWebSocketsColumn(column)),
    resetColumns: () => dispatch(Actions.resetWebSocketsColumns()),
  })
)(FrameListHeader);
