/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Services = require("Services");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { PluralForm } = require("devtools/shared/plural-form");
const { getDisplayedFrames } = require("../../selectors/index");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { table, tbody, tr, td, div, input, label, hr } = dom;
const { L10N } = require("../../utils/l10n");
const FRAMES_EMPTY_TEXT = L10N.getStr("messagesEmptyText");
const TOGGLE_MESSAGES_TRUNCATION = L10N.getStr("toggleMessagesTruncation");
const TOGGLE_MESSAGES_TRUNCATION_TITLE = L10N.getStr(
  "toggleMessagesTruncation.title"
);
const Actions = require("../../actions/index");

const { getSelectedFrame } = require("../../selectors/index");

// Components
const FrameListContextMenu = require("devtools/client/netmonitor/src/components/websockets/FrameListContextMenu");
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
      startPanelContainer: PropTypes.object,
      frames: PropTypes.array,
      selectedFrame: PropTypes.object,
      selectFrame: PropTypes.func.isRequired,
      columns: PropTypes.object.isRequired,
      channelId: PropTypes.number,
      onSelectFrameDelta: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onContextMenu = this.onContextMenu.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.framesLimit = Services.prefs.getIntPref(
      "devtools.netmonitor.ws.displayed-frames.limit"
    );
    this.currentTruncatedNum = 0;
    this.state = {
      checked: false,
    };
    this.pinnedToBottom = false;
    this.initIntersectionObserver = false;
    this.intersectionObserver = null;
    this.toggleTruncationCheckBox = this.toggleTruncationCheckBox.bind(this);
  }

  componentDidMount() {
    const { startPanelContainer } = this.props;
    const scrollAnchor = this.refs.scrollAnchor;

    if (scrollAnchor) {
      // Always scroll to anchor when FrameListContent component first mounts.
      scrollAnchor.scrollIntoView();
    }
    this.setupScrollToBottom(startPanelContainer, scrollAnchor);
  }

  componentDidUpdate(prevProps) {
    const { startPanelContainer, channelId, frames } = this.props;
    const scrollAnchor = this.refs.scrollAnchor;

    // When frames are cleared, the previous scrollAnchor would be destroyed, so we need to reset this boolean.
    if (!scrollAnchor) {
      this.initIntersectionObserver = false;
    }

    // If a new WebSocket connection is selected, scroll to anchor.
    if (channelId !== prevProps.channelId && scrollAnchor) {
      scrollAnchor.scrollIntoView();
    }

    // Do not autoscroll if the selection changed. This would cause
    // the newly selected frame to jump just after clicking in.
    // (not user friendly)
    //
    // If the selection changed, we need to ensure that the newly
    // selected frame is properly scrolled into the visible area.
    if (prevProps.selectedFrame === this.props.selectedFrame) {
      this.setupScrollToBottom(startPanelContainer, scrollAnchor);
    } else {
      const head = document.querySelector("thead.ws-frames-list-headers-group");
      const selectedRow = document.querySelector(
        "tr.ws-frame-list-item.selected"
      );

      if (selectedRow) {
        const rowRect = selectedRow.getBoundingClientRect();
        const scrollableRect = startPanelContainer.getBoundingClientRect();
        const headRect = head.getBoundingClientRect();

        if (rowRect.top <= scrollableRect.top) {
          selectedRow.scrollIntoView(true);

          // We need to scroll a bit more to get the row out
          // of the header. The header is sticky and overlaps
          // part of the scrollable area.
          startPanelContainer.scrollTop -= headRect.height;
        } else if (rowRect.bottom > scrollableRect.bottom) {
          selectedRow.scrollIntoView(false);
        }
      }
    }
  }

  componentWillUnmount() {
    // Reset observables and boolean values.
    const scrollAnchor = this.refs.scrollAnchor;

    if (this.intersectionObserver) {
      this.intersectionObserver.unobserve(scrollAnchor);
      this.initIntersectionObserver = false;
      this.pinnedToBottom = false;
    }
  }

  setupScrollToBottom(startPanelContainer, scrollAnchor) {
    if (startPanelContainer && scrollAnchor) {
      // Initialize intersection observer.
      if (!this.initIntersectionObserver) {
        this.intersectionObserver = new IntersectionObserver(
          () => {
            // When scrollAnchor first comes into view, this.pinnedToBottom is set to true.
            // When the anchor goes out of view, this callback function triggers again and toggles this.pinnedToBottom.
            // Subsequent scroll into/out of view will toggle this.pinnedToBottom.
            this.pinnedToBottom = !this.pinnedToBottom;
          },
          {
            root: startPanelContainer,
            threshold: 0.1,
          }
        );
        if (this.intersectionObserver) {
          this.intersectionObserver.observe(scrollAnchor);
          this.initIntersectionObserver = true;
        }
      }

      if (this.pinnedToBottom) {
        scrollAnchor.scrollIntoView();
      }
    }
  }

  toggleTruncationCheckBox() {
    this.setState({
      checked: !this.state.checked,
    });
  }

  onMouseDown(evt, item) {
    if (evt.button === LEFT_MOUSE_BUTTON) {
      this.props.selectFrame(item);
    }
  }

  onContextMenu(evt, item) {
    evt.preventDefault();
    const { connector } = this.props;
    this.contextMenu = new FrameListContextMenu({
      connector,
    });
    this.contextMenu.open(evt, item);
  }

  /**
   * Handler for keyboard events. For arrow up/down, page up/down, home/end,
   * move the selection up or down.
   */
  onKeyDown(evt) {
    evt.preventDefault();
    evt.stopPropagation();
    let delta;

    switch (evt.key) {
      case "ArrowUp":
      case "ArrowLeft":
        delta = -1;
        break;
      case "ArrowDown":
      case "ArrowRight":
        delta = +1;
        break;
      case "PageUp":
        delta = "PAGE_UP";
        break;
      case "PageDown":
        delta = "PAGE_DOWN";
        break;
      case "Home":
        delta = -Infinity;
        break;
      case "End":
        delta = +Infinity;
        break;
    }

    if (delta) {
      this.props.onSelectFrameDelta(delta);
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

    let displayedFrames;
    let MESSAGES_TRUNCATED;
    const shouldTruncate = frames.length > this.framesLimit;
    if (shouldTruncate) {
      // If the checkbox is checked, we display all frames after the currentedTruncatedNum limit.
      // If the checkbox is unchecked, we display all frames after the framesLimit.
      this.currentTruncatedNum = this.state.checked
        ? this.currentTruncatedNum
        : frames.length - this.framesLimit;
      displayedFrames = frames.slice(this.currentTruncatedNum);

      MESSAGES_TRUNCATED = PluralForm.get(
        this.currentTruncatedNum,
        L10N.getStr("netmonitor.ws.truncated-messages.warning")
      ).replace("#1", this.currentTruncatedNum);
    } else {
      displayedFrames = frames;
    }

    return div(
      {},
      table(
        { className: "ws-frames-list-table" },
        FrameListHeader(),
        tbody(
          {
            className: "ws-frames-list-body",
            onKeyDown: this.onKeyDown,
          },
          tr(
            {
              tabIndex: 0,
            },
            td(
              {
                className: "truncated-messages-cell",
                colSpan: visibleColumns.length,
              },
              shouldTruncate &&
                div(
                  {
                    className: "truncated-messages-header",
                  },
                  div(
                    {
                      className: "truncated-messages-container",
                    },
                    div({
                      className: "truncated-messages-warning-icon",
                    }),
                    div(
                      {
                        className: "truncated-message",
                        title: MESSAGES_TRUNCATED,
                      },
                      MESSAGES_TRUNCATED
                    )
                  ),
                  label(
                    {
                      className: "truncated-messages-checkbox-label",
                      title: TOGGLE_MESSAGES_TRUNCATION_TITLE,
                    },
                    input({
                      type: "checkbox",
                      className: "truncation-checkbox",
                      title: TOGGLE_MESSAGES_TRUNCATION_TITLE,
                      checked: this.state.checked,
                      onChange: this.toggleTruncationCheckBox,
                    }),
                    TOGGLE_MESSAGES_TRUNCATION
                  )
                )
            )
          ),
          displayedFrames.map((item, index) =>
            FrameListItem({
              key: "ws-frame-list-item-" + index,
              item,
              index,
              isSelected: item === selectedFrame,
              onMouseDown: evt => this.onMouseDown(evt, item),
              onContextMenu: evt => this.onContextMenu(evt, item),
              connector,
              visibleColumns,
            })
          )
        )
      ),
      hr({
        ref: "scrollAnchor",
        className: "ws-frames-list-scroll-anchor",
      })
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
    onSelectFrameDelta: delta => dispatch(Actions.selectFrameDelta(delta)),
  })
)(FrameListContent);
