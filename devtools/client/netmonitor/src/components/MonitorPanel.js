/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const Actions = require("devtools/client/netmonitor/src/actions/index");
const {
  updateFormDataSections,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  getSelectedRequest,
  isSelectedRequestVisible,
} = require("devtools/client/netmonitor/src/selectors/index");

// Components
const SplitBox = createFactory(
  require("devtools/client/shared/components/splitter/SplitBox")
);
const RequestList = createFactory(
  require("devtools/client/netmonitor/src/components/request-list/RequestList")
);
const Toolbar = createFactory(
  require("devtools/client/netmonitor/src/components/Toolbar")
);

loader.lazyGetter(this, "NetworkDetailsBar", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-details/NetworkDetailsBar")
  );
});

loader.lazyGetter(this, "NetworkActionBar", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/NetworkActionBar")
  );
});

// MediaQueryList object responsible for switching sidebar splitter
// between landscape and portrait mode (depending on browser window size).
const MediaQueryVert = window.matchMedia("(min-width: 700px)");

// MediaQueryList object responsible for switching the toolbar
// between single and 2-rows layout (depending on browser window size).
const MediaQuerySingleRow = window.matchMedia("(min-width: 1020px)");

/**
 * Monitor panel component
 * The main panel for displaying various network request information
 */
class MonitorPanel extends Component {
  static get propTypes() {
    return {
      actions: PropTypes.object.isRequired,
      connector: PropTypes.object.isRequired,
      isEmpty: PropTypes.bool.isRequired,
      networkDetailsOpen: PropTypes.bool.isRequired,
      openNetworkDetails: PropTypes.func.isRequired,
      toolboxDoc: PropTypes.object.isRequired,
      // Callback for opening split console.
      openSplitConsole: PropTypes.func,
      onNetworkDetailsResized: PropTypes.func.isRequired,
      request: PropTypes.object,
      selectedRequestVisible: PropTypes.bool.isRequired,
      sourceMapURLService: PropTypes.object,
      openLink: PropTypes.func,
      updateRequest: PropTypes.func.isRequired,
      networkActionOpen: PropTypes.bool.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      isSingleRow: MediaQuerySingleRow.matches,
      isVerticalSpliter: MediaQueryVert.matches,
    };

    this.onLayoutChange = this.onLayoutChange.bind(this);
    this.onNetworkDetailsResized = this.onNetworkDetailsResized.bind(this);
  }

  componentDidMount() {
    MediaQuerySingleRow.addListener(this.onLayoutChange);
    MediaQueryVert.addListener(this.onLayoutChange);
    this.persistDetailsPanelSize();
    this.persistActionBarSize();
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    updateFormDataSections(nextProps);
  }

  componentDidUpdate() {
    const { selectedRequestVisible, openNetworkDetails } = this.props;
    if (!selectedRequestVisible) {
      openNetworkDetails(false);
    }
  }

  componentWillUnmount() {
    MediaQuerySingleRow.removeListener(this.onLayoutChange);
    MediaQueryVert.removeListener(this.onLayoutChange);
    this.persistDetailsPanelSize();
    this.persistActionBarSize();
  }

  persistDetailsPanelSize() {
    const { clientWidth, clientHeight } = findDOMNode(this.refs.endPanel) || {};

    if (this.state.isVerticalSpliter && clientWidth) {
      Services.prefs.setIntPref(
        "devtools.netmonitor.panes-network-details-width",
        clientWidth
      );
    }
    if (!this.state.isVerticalSpliter && clientHeight) {
      Services.prefs.setIntPref(
        "devtools.netmonitor.panes-network-details-height",
        clientHeight
      );
    }
  }

  persistActionBarSize() {
    const { clientWidth, clientHeight } =
      findDOMNode(this.refs.actionBar) || {};
    if (clientWidth) {
      Services.prefs.setIntPref(
        "devtools.netmonitor.panes-search-width",
        clientWidth
      );
    }
    if (clientHeight) {
      Services.prefs.setIntPref(
        "devtools.netmonitor.panes-search-height",
        clientHeight
      );
    }
  }

  onLayoutChange() {
    this.setState({
      isSingleRow: MediaQuerySingleRow.matches,
      isVerticalSpliter: MediaQueryVert.matches,
    });
  }

  onNetworkDetailsResized(width, height) {
    // Cleaning width and height parameters, because SplitBox passes ALWAYS two values,
    // while depending on orientation ONLY ONE dimension is managed by it at a time.
    const { isVerticalSpliter } = this.state;
    return this.props.onNetworkDetailsResized(
      isVerticalSpliter ? width : null,
      isVerticalSpliter ? null : height
    );
  }

  renderActionBar() {
    const { connector, isEmpty, networkActionOpen } = this.props;

    const initialWidth = Services.prefs.getIntPref(
      "devtools.netmonitor.panes-search-width"
    );
    const initialHeight = Services.prefs.getIntPref(
      "devtools.netmonitor.panes-search-height"
    );

    return SplitBox({
      className: "devtools-responsive-container",
      initialWidth,
      initialHeight,
      minSize: "250px",
      maxSize: "80%",
      splitterSize: networkActionOpen ? 1 : 0,
      startPanel:
        networkActionOpen &&
        NetworkActionBar({
          ref: "actionBar",
          connector,
        }),
      endPanel: RequestList({ isEmpty, connector }),
      endPanelControl: false,
      vert: true,
    });
  }

  render() {
    const {
      actions,
      connector,
      networkDetailsOpen,
      openLink,
      openSplitConsole,
      sourceMapURLService,
      toolboxDoc,
    } = this.props;

    const initialWidth = Services.prefs.getIntPref(
      "devtools.netmonitor.panes-network-details-width"
    );

    const initialHeight = Services.prefs.getIntPref(
      "devtools.netmonitor.panes-network-details-height"
    );

    return div(
      { className: "monitor-panel" },
      Toolbar({
        actions,
        connector,
        openSplitConsole,
        singleRow: this.state.isSingleRow,
        toolboxDoc,
      }),
      SplitBox({
        className: "devtools-responsive-container",
        initialWidth,
        initialHeight,
        minSize: "50px",
        maxSize: "80%",
        splitterSize: networkDetailsOpen ? 1 : 0,
        startPanel: this.renderActionBar(),
        endPanel:
          networkDetailsOpen &&
          NetworkDetailsBar({
            ref: "endPanel",
            connector,
            openLink,
            sourceMapURLService,
          }),
        endPanelCollapsed: !networkDetailsOpen,
        endPanelControl: true,
        vert: this.state.isVerticalSpliter,
        onControlledPanelResized: this.onNetworkDetailsResized,
      })
    );
  }
}

module.exports = connect(
  state => ({
    isEmpty: !state.requests.requests.length,
    networkDetailsOpen: state.ui.networkDetailsOpen,
    networkActionOpen: state.ui.networkActionOpen,
    request: getSelectedRequest(state),
    selectedRequestVisible: isSelectedRequestVisible(state),
  }),
  dispatch => ({
    openNetworkDetails: open => dispatch(Actions.openNetworkDetails(open)),
    onNetworkDetailsResized: (width, height) =>
      dispatch(Actions.resizeNetworkDetails(width, height)),
    updateRequest: (id, data, batch) =>
      dispatch(Actions.updateRequest(id, data, batch)),
  })
)(MonitorPanel);
