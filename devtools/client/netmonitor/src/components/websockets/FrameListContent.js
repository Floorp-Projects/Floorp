/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { getDisplayedFrames } = require("../../selectors/index");

const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { table, tbody, div } = dom;

const { L10N } = require("../../utils/l10n");
const FRAMES_EMPTY_TEXT = L10N.getStr("messagesEmptyText");
const Actions = require("../../actions/index");

const { getSelectedFrame } = require("../../selectors/index");

loader.lazyGetter(this, "FrameListHeader", function() {
  return createFactory(require("./FrameListHeader"));
});
loader.lazyGetter(this, "FrameListItem", function() {
  return createFactory(require("./FrameListItem"));
});

const LEFT_MOUSE_BUTTON = 0;

/**
 * Renders the actual contents of the WebSocket frame list.
 */
class FrameListContent extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      frames: PropTypes.array,
      selectedFrame: PropTypes.object,
      selectFrame: PropTypes.func.isRequired,
      columns: PropTypes.object.isRequired,
    };
  }

  onMouseDown(evt, item) {
    if (evt.button === LEFT_MOUSE_BUTTON) {
      this.props.selectFrame(item);
    }
  }

  render() {
    const { frames, selectedFrame, connector, columns } = this.props;

    if (frames.length === 0) {
      return div(
        { className: "empty-notice ws-frame-list-empty-notice" },
        FRAMES_EMPTY_TEXT
      );
    }

    const visibleColumns = Object.entries(columns)
      .filter(([name, isVisible]) => isVisible)
      .map(([name]) => name);

    return table(
      { className: "ws-frames-list-table" },
      FrameListHeader(),
      tbody(
        {
          className: "ws-frames-list-body",
        },
        frames.map((item, index) =>
          FrameListItem({
            key: "ws-frame-list-item-" + index,
            item,
            index,
            isSelected: item === selectedFrame,
            onMouseDown: evt => this.onMouseDown(evt, item),
            connector,
            visibleColumns,
          })
        )
      )
    );
  }
}

module.exports = connect(
  state => ({
    selectedFrame: getSelectedFrame(state),
    frames: getDisplayedFrames(state),
    columns: state.webSockets.columns,
  }),
  dispatch => ({
    selectFrame: item => dispatch(Actions.selectFrame(item)),
  })
)(FrameListContent);
