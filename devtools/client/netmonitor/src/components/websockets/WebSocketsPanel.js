/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  Component,
  createRef,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const Actions = require("devtools/client/netmonitor/src/actions/index");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const {
  getSelectedFrame,
  isSelectedFrameVisible,
} = require("devtools/client/netmonitor/src/selectors/index");

// Components
const SplitBox = createFactory(
  require("devtools/client/shared/components/splitter/SplitBox")
);
const FrameListContent = createFactory(
  require("devtools/client/netmonitor/src/components/websockets/FrameListContent")
);
const Toolbar = createFactory(
  require("devtools/client/netmonitor/src/components/websockets/Toolbar")
);
const StatusBar = createFactory(
  require("devtools/client/netmonitor/src/components/websockets/StatusBar")
);

loader.lazyGetter(this, "FramePayload", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/websockets/FramePayload")
  );
});

/**
 * Renders a list of WebSocket frames in table view.
 * Full payload is separated using a SplitBox.
 */
class WebSocketsPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      selectedFrame: PropTypes.object,
      frameDetailsOpen: PropTypes.bool.isRequired,
      openFrameDetailsTab: PropTypes.func.isRequired,
      selectedFrameVisible: PropTypes.bool.isRequired,
      channelId: PropTypes.number,
    };
  }

  constructor(props) {
    super(props);

    this.searchboxRef = createRef();
    this.clearFilterText = this.clearFilterText.bind(this);
    this.handleContainerElement = this.handleContainerElement.bind(this);
    this.state = {
      startPanelContainer: null,
    };
  }

  componentDidUpdate(prevProps) {
    const { selectedFrameVisible, openFrameDetailsTab, channelId } = this.props;

    // If a new WebSocket connection is selected, clear the filter text
    if (channelId !== prevProps.channelId) {
      this.clearFilterText();
    }

    if (!selectedFrameVisible) {
      openFrameDetailsTab(false);
    }
  }

  componentWillUnmount() {
    const { openFrameDetailsTab } = this.props;
    openFrameDetailsTab(false);

    const { clientHeight } = findDOMNode(this.refs.endPanel) || {};

    if (clientHeight) {
      Services.prefs.setIntPref(
        "devtools.netmonitor.ws.payload-preview-height",
        clientHeight
      );
    }
  }

  /* Store the parent DOM element of the SplitBox startPanel's element.
     We need this element for as an option for the IntersectionObserver */
  handleContainerElement(element) {
    if (!this.state.startPanelContainer) {
      this.setState({
        startPanelContainer: element,
      });
    }
  }

  // Reset the filter text
  clearFilterText() {
    if (this.searchboxRef) {
      this.searchboxRef.current.onClearButtonClick();
    }
  }

  render() {
    const {
      frameDetailsOpen,
      connector,
      selectedFrame,
      channelId,
    } = this.props;

    const { searchboxRef } = this;
    const { startPanelContainer } = this.state;

    const initialHeight = Services.prefs.getIntPref(
      "devtools.netmonitor.ws.payload-preview-height"
    );

    return div(
      { className: "monitor-panel" },
      Toolbar({
        searchboxRef,
      }),
      SplitBox({
        className: "devtools-responsive-container",
        initialHeight: initialHeight,
        minSize: "50px",
        maxSize: "80%",
        splitterSize: frameDetailsOpen ? 1 : 0,
        onSelectContainerElement: this.handleContainerElement,
        startPanel: FrameListContent({
          connector,
          startPanelContainer,
          channelId,
        }),
        endPanel:
          frameDetailsOpen &&
          FramePayload({
            ref: "endPanel",
            connector,
            selectedFrame,
          }),
        endPanelCollapsed: !frameDetailsOpen,
        endPanelControl: true,
        vert: false,
      }),
      StatusBar()
    );
  }
}

module.exports = connect(
  state => ({
    channelId: state.webSockets.currentChannelId,
    frameDetailsOpen: state.webSockets.frameDetailsOpen,
    selectedFrame: getSelectedFrame(state),
    selectedFrameVisible: isSelectedFrameVisible(state),
  }),
  dispatch => ({
    openFrameDetailsTab: open => dispatch(Actions.openFrameDetails(open)),
  })
)(WebSocketsPanel);
