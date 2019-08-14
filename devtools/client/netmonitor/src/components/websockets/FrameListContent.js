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
    };
  }

  constructor(props) {
    super(props);

    this.framesLimit = Services.prefs.getIntPref(
      "devtools.netmonitor.ws.displayed-frames.limit"
    );
    this.currentTruncatedNum = 0;
    this.state = {
      checked: false,
    };
    this.pinnedToBottom = false;
    this.initIntersectionObserver = false;
  }

  componentDidUpdate() {
    const { startPanelContainer } = this.props;
    const scrollAnchor = this.refs.scrollAnchor;

    // When frames are cleared, the previous scrollAnchor would be destroyed, so we need to reset this boolean.
    if (!scrollAnchor) {
      this.initIntersectionObserver = false;
    }

    if (startPanelContainer && scrollAnchor) {
      // Initialize intersection observer.
      if (!this.initIntersectionObserver) {
        const observer = new IntersectionObserver(
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
        observer.observe(scrollAnchor);
        this.initIntersectionObserver = true;
      }

      if (this.pinnedToBottom) {
        scrollAnchor.scrollIntoView();
      }
    }
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
                    onClick: () => {
                      this.setState({
                        checked: !this.state.checked,
                      });
                    },
                  }),
                  TOGGLE_MESSAGES_TRUNCATION
                )
              )
          )
        ),
        tbody(
          {
            className: "ws-frames-list-body",
          },
          displayedFrames.map((item, index) =>
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
  })
)(FrameListContent);
