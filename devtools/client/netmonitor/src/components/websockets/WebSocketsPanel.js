/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const Actions = require("../../actions/index");

const {
  getSelectedFrame,
  isSelectedFrameVisible,
} = require("../../selectors/index");

// Components
const SplitBox = createFactory(
  require("devtools/client/shared/components/splitter/SplitBox")
);
const FrameListContent = createFactory(require("./FrameListContent"));

loader.lazyGetter(this, "FramePayload", function() {
  return createFactory(require("./FramePayload"));
});

/**
 * Renders a list of WebSocket frames in table view.
 * Full payload is separated using a SplitBox.
 */
class WebSocketsPanel extends Component {
  static get propTypes() {
    return {
      channelId: PropTypes.number,
      connector: PropTypes.object.isRequired,
      selectedFrame: PropTypes.object,
      frameDetailsOpen: PropTypes.bool.isRequired,
      openFrameDetailsTab: PropTypes.func.isRequired,
      selectedFrameVisible: PropTypes.bool.isRequired,
    };
  }

  constructor(props) {
    super(props);
  }

  componentDidUpdate() {
    const { selectedFrameVisible, openFrameDetailsTab } = this.props;
    if (!selectedFrameVisible) {
      openFrameDetailsTab(false);
    }
  }

  render() {
    const {
      frameDetailsOpen,
      channelId,
      connector,
      selectedFrame,
    } = this.props;

    const initialWidth = Services.prefs.getIntPref(
      "devtools.netmonitor.ws.payload-preview-width"
    );
    const initialHeight = Services.prefs.getIntPref(
      "devtools.netmonitor.ws.payload-preview-height"
    );

    return SplitBox({
      className: "devtools-responsive-container",
      initialWidth: initialWidth,
      initialHeight: initialHeight,
      minSize: "50px",
      maxSize: "50%",
      splitterSize: frameDetailsOpen ? 1 : 0,
      startPanel: FrameListContent({ channelId, connector }),
      endPanel:
        frameDetailsOpen &&
        FramePayload({
          connector,
          selectedFrame,
        }),
      endPanelCollapsed: !frameDetailsOpen,
      endPanelControl: true,
      vert: false,
    });
  }
}

module.exports = connect(
  (state, props) => ({
    selectedFrame: getSelectedFrame(state),
    frameDetailsOpen: state.webSockets.frameDetailsOpen,
    selectedFrameVisible: isSelectedFrameVisible(
      state,
      props.channelId,
      getSelectedFrame(state)
    ),
  }),
  dispatch => ({
    openFrameDetailsTab: open => dispatch(Actions.openFrameDetails(open)),
  })
)(WebSocketsPanel);
