/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Actions = require("devtools/client/netmonitor/src/actions/index");
const {
  FILTER_SEARCH_DELAY,
} = require("devtools/client/netmonitor/src/constants");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
const { button, span, div } = dom;

// Components
const FrameFilterMenu = createFactory(require("./FrameFilterMenu"));
const SearchBox = createFactory(
  require("devtools/client/shared/components/SearchBox")
);

// Localization
const WS_TOOLBAR_CLEAR = L10N.getStr("netmonitor.ws.toolbar.clear");
const WS_SEARCH_KEY_SHORTCUT = L10N.getStr(
  "netmonitor.ws.toolbar.filterFreetext.key"
);
const WS_SEARCH_PLACE_HOLDER = L10N.getStr(
  "netmonitor.ws.toolbar.filterFreetext.label"
);

/**
 * WebSocketsPanel toolbar component.
 *
 * Toolbar contains a set of useful tools that clear the list of
 * existing frames as well as filter content.
 */
class Toolbar extends Component {
  static get propTypes() {
    return {
      searchboxRef: PropTypes.object.isRequired,
      toggleFrameFilterType: PropTypes.func.isRequired,
      clearFrames: PropTypes.func.isRequired,
      setFrameFilterText: PropTypes.func.isRequired,
      frameFilterType: PropTypes.string.isRequired,
    };
  }

  componentWillUnmount() {
    const { setFrameFilterText } = this.props;
    setFrameFilterText("");
  }

  /**
   * Render a separator.
   */
  renderSeparator() {
    return span({ className: "devtools-separator" });
  }

  /**
   * Render a clear button.
   */
  renderClearButton(clearFrames) {
    return button({
      className:
        "devtools-button devtools-clear-icon ws-frames-list-clear-button",
      title: WS_TOOLBAR_CLEAR,
      onClick: () => {
        clearFrames();
      },
    });
  }

  /**
   * Render the frame filter menu button.
   */
  renderFrameFilterMenu() {
    const { frameFilterType, toggleFrameFilterType } = this.props;

    return FrameFilterMenu({
      frameFilterType,
      toggleFrameFilterType,
    });
  }

  /**
   * Render filter Searchbox.
   */
  renderFilterBox(setFrameFilterText) {
    return SearchBox({
      delay: FILTER_SEARCH_DELAY,
      keyShortcut: WS_SEARCH_KEY_SHORTCUT,
      placeholder: WS_SEARCH_PLACE_HOLDER,
      type: "filter",
      ref: this.props.searchboxRef,
      onChange: setFrameFilterText,
    });
  }

  render() {
    const { clearFrames, setFrameFilterText } = this.props;

    return div(
      {
        id: "netmonitor-toolbar-container",
        className: "devtools-toolbar devtools-input-toolbar",
      },
      this.renderClearButton(clearFrames),
      this.renderSeparator(),
      this.renderFrameFilterMenu(),
      this.renderSeparator(),
      this.renderFilterBox(setFrameFilterText)
    );
  }
}

module.exports = connect(
  state => ({
    frameFilterType: state.webSockets.frameFilterType,
  }),
  dispatch => ({
    clearFrames: () => dispatch(Actions.clearFrames()),
    toggleFrameFilterType: filter =>
      dispatch(Actions.toggleFrameFilterType(filter)),
    setFrameFilterText: text => dispatch(Actions.setFrameFilterText(text)),
  })
)(Toolbar);
