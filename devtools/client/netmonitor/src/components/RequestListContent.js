/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const {
  HTMLTooltip,
} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");

const Actions = require("../actions/index");
const { formDataURI } = require("../utils/request-utils");
const {
  getDisplayedRequests,
  getSelectedRequest,
  getWaterfallScale,
} = require("../selectors/index");

loader.lazyRequireGetter(
  this,
  "openRequestInTab",
  "devtools/client/netmonitor/src/utils/firefox/open-request-in-tab",
  true
);
loader.lazyGetter(this, "setImageTooltip", function() {
  return require("devtools/client/shared/widgets/tooltip/ImageTooltipHelper")
    .setImageTooltip;
});
loader.lazyGetter(this, "getImageDimensions", function() {
  return require("devtools/client/shared/widgets/tooltip/ImageTooltipHelper")
    .getImageDimensions;
});

// Components
const RequestListHeader = createFactory(require("./RequestListHeader"));
const RequestListItem = createFactory(require("./RequestListItem"));
const RequestListContextMenu = require("../widgets/RequestListContextMenu");

const { div } = dom;

// Tooltip show / hide delay in ms
const REQUESTS_TOOLTIP_TOGGLE_DELAY = 500;
// Tooltip image maximum dimension in px
const REQUESTS_TOOLTIP_IMAGE_MAX_DIM = 400;
// Gecko's scrollTop is int32_t, so the maximum value is 2^31 - 1 = 2147483647
const MAX_SCROLL_HEIGHT = 2147483647;

const LEFT_MOUSE_BUTTON = 0;
const RIGHT_MOUSE_BUTTON = 2;

/**
 * Renders the actual contents of the request list.
 */
class RequestListContent extends Component {
  static get propTypes() {
    return {
      blockSelectedRequestURL: PropTypes.func.isRequired,
      connector: PropTypes.object.isRequired,
      columns: PropTypes.object.isRequired,
      networkDetailsOpen: PropTypes.bool.isRequired,
      networkDetailsWidth: PropTypes.number,
      networkDetailsHeight: PropTypes.number,
      cloneRequest: PropTypes.func.isRequired,
      clickedRequest: PropTypes.object,
      openDetailsPanelTab: PropTypes.func.isRequired,
      sendCustomRequest: PropTypes.func.isRequired,
      displayedRequests: PropTypes.array.isRequired,
      firstRequestStartedMs: PropTypes.number.isRequired,
      fromCache: PropTypes.bool,
      onCauseBadgeMouseDown: PropTypes.func.isRequired,
      onItemRightMouseButtonDown: PropTypes.func.isRequired,
      onItemMouseDown: PropTypes.func.isRequired,
      onSecurityIconMouseDown: PropTypes.func.isRequired,
      onSelectDelta: PropTypes.func.isRequired,
      onWaterfallMouseDown: PropTypes.func.isRequired,
      openStatistics: PropTypes.func.isRequired,
      scale: PropTypes.number,
      selectRequest: PropTypes.func.isRequired,
      selectedRequest: PropTypes.object,
      unblockSelectedRequestURL: PropTypes.func.isRequired,
      requestFilterTypes: PropTypes.object.isRequired,
      panelOpen: PropTypes.bool,
      toggleSearchPanel: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.isScrolledToBottom = this.isScrolledToBottom.bind(this);
    this.onHover = this.onHover.bind(this);
    this.onScroll = this.onScroll.bind(this);
    this.onResize = this.onResize.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.openRequestInTab = this.openRequestInTab.bind(this);
    this.onDoubleClick = this.onDoubleClick.bind(this);
    this.onContextMenu = this.onContextMenu.bind(this);
    this.onFocusedNodeChange = this.onFocusedNodeChange.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
  }

  componentWillMount() {
    this.tooltip = new HTMLTooltip(window.parent.document, { type: "arrow" });
    window.addEventListener("resize", this.onResize);
  }

  componentDidMount() {
    // Install event handler for displaying a tooltip
    this.tooltip.startTogglingOnHover(this.refs.scrollEl, this.onHover, {
      toggleDelay: REQUESTS_TOOLTIP_TOGGLE_DELAY,
      interactive: true,
    });
    // Install event handler to hide the tooltip on scroll
    this.refs.scrollEl.addEventListener("scroll", this.onScroll, true);
    this.onResize();
  }

  componentWillUpdate(nextProps) {
    // Check if the list is scrolled to bottom before the UI update.
    this.shouldScrollBottom = this.isScrolledToBottom();
  }

  componentDidUpdate(prevProps) {
    const node = this.refs.scrollEl;
    // Keep the list scrolled to bottom if a new row was added
    if (this.shouldScrollBottom && node.scrollTop !== MAX_SCROLL_HEIGHT) {
      // Using maximum scroll height rather than node.scrollHeight to avoid sync reflow.
      node.scrollTop = MAX_SCROLL_HEIGHT;
    }
    if (
      prevProps.networkDetailsOpen !== this.props.networkDetailsOpen ||
      prevProps.networkDetailsWidth !== this.props.networkDetailsWidth ||
      prevProps.networkDetailsHeight !== this.props.networkDetailsHeight
    ) {
      this.onResize();
    }
  }

  componentWillUnmount() {
    this.refs.scrollEl.removeEventListener("scroll", this.onScroll, true);

    // Uninstall the tooltip event handler
    this.tooltip.stopTogglingOnHover();
    window.removeEventListener("resize", this.onResize);
  }

  /*
   * Removing onResize() method causes perf regression - too many repaints of the panel.
   * So it is needed in ComponentDidMount and ComponentDidUpdate. See Bug 1532914.
   */
  onResize() {
    const parent = this.refs.scrollEl.parentNode;
    this.refs.scrollEl.style.width = parent.offsetWidth + "px";
    this.refs.scrollEl.style.height = parent.offsetHeight + "px";
  }

  isScrolledToBottom() {
    const { scrollEl, rowGroupEl } = this.refs;
    const lastChildEl = rowGroupEl.lastElementChild;

    if (!lastChildEl) {
      return false;
    }

    const lastNodeHeight = lastChildEl.clientHeight;
    return (
      scrollEl.scrollTop + scrollEl.clientHeight >=
      scrollEl.scrollHeight - lastNodeHeight / 2
    );
  }

  /**
   * The predicate used when deciding whether a popup should be shown
   * over a request item or not.
   *
   * @param Node target
   *        The element node currently being hovered.
   * @param object tooltip
   *        The current tooltip instance.
   * @return {Promise}
   */
  async onHover(target, tooltip) {
    const itemEl = target.closest(".request-list-item");
    if (!itemEl) {
      return false;
    }
    const itemId = itemEl.dataset.id;
    if (!itemId) {
      return false;
    }
    const requestItem = this.props.displayedRequests.find(r => r.id == itemId);
    if (!requestItem) {
      return false;
    }

    if (!target.closest(".requests-list-file")) {
      return false;
    }

    const { mimeType } = requestItem;
    if (!mimeType || !mimeType.includes("image/")) {
      return false;
    }

    const responseContent = await this.props.connector.requestData(
      requestItem.id,
      "responseContent"
    );
    const { encoding, text } = responseContent.content;
    const src = formDataURI(mimeType, encoding, text);
    const maxDim = REQUESTS_TOOLTIP_IMAGE_MAX_DIM;
    const { naturalWidth, naturalHeight } = await getImageDimensions(
      tooltip.doc,
      src
    );
    const options = { maxDim, naturalWidth, naturalHeight };
    setImageTooltip(tooltip, tooltip.doc, src, options);

    return itemEl.querySelector(".requests-list-file");
  }

  /**
   * Scroll listener for the requests menu view.
   */
  onScroll() {
    this.tooltip.hide();
  }

  onMouseDown(evt, id, channelId) {
    if (evt.button === LEFT_MOUSE_BUTTON) {
      this.props.selectRequest(id, channelId);
    } else if (evt.button === RIGHT_MOUSE_BUTTON) {
      this.props.onItemRightMouseButtonDown(id);
    }
  }

  /**
   * Handler for keyboard events. For arrow up/down, page up/down, home/end,
   * move the selection up or down.
   */
  onKeyDown(evt) {
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
      // Prevent scrolling when pressing navigation keys.
      evt.preventDefault();
      evt.stopPropagation();
      this.props.onSelectDelta(delta);
    }
  }

  /**
   * Opens selected item in a new tab.
   */
  async openRequestInTab(id, url, requestHeaders, requestPostData) {
    requestHeaders =
      requestHeaders ||
      (await this.props.connector.requestData(id, "requestHeaders"));

    requestPostData =
      requestPostData ||
      (await this.props.connector.requestData(id, "requestPostData"));

    openRequestInTab(url, requestHeaders, requestPostData);
  }

  onDoubleClick({ id, url, requestHeaders, requestPostData }) {
    this.openRequestInTab(id, url, requestHeaders, requestPostData);
  }

  onContextMenu(evt) {
    evt.preventDefault();
    const { clickedRequest, displayedRequests } = this.props;

    if (!this.contextMenu) {
      const {
        blockSelectedRequestURL,
        connector,
        cloneRequest,
        openDetailsPanelTab,
        sendCustomRequest,
        openStatistics,
        unblockSelectedRequestURL,
        toggleSearchPanel,
        panelOpen,
      } = this.props;
      this.contextMenu = new RequestListContextMenu({
        blockSelectedRequestURL,
        connector,
        cloneRequest,
        openDetailsPanelTab,
        sendCustomRequest,
        openStatistics,
        openRequestInTab: this.openRequestInTab,
        unblockSelectedRequestURL,
        toggleSearchPanel,
        panelOpen,
      });
    }

    this.contextMenu.open(evt, clickedRequest, displayedRequests);
  }

  /**
   * If selection has just changed (by keyboard navigation), don't keep the list
   * scrolled to bottom, but allow scrolling up with the selection.
   */
  onFocusedNodeChange() {
    this.shouldScrollBottom = false;
  }

  render() {
    const {
      connector,
      columns,
      displayedRequests,
      firstRequestStartedMs,
      onCauseBadgeMouseDown,
      onSecurityIconMouseDown,
      onWaterfallMouseDown,
      requestFilterTypes,
      scale,
      selectedRequest,
    } = this.props;

    return div(
      {
        ref: "scrollEl",
        className: "requests-list-scroll",
      },
      dom.table(
        {
          className: "requests-list-table",
        },
        RequestListHeader(),
        dom.tbody(
          {
            ref: "rowGroupEl",
            className: "requests-list-row-group",
            tabIndex: 0,
            onKeyDown: this.onKeyDown,
            style: {
              "--timings-scale": scale,
              "--timings-rev-scale": 1 / scale,
            },
          },
          displayedRequests.map((item, index) =>
            RequestListItem({
              blocked: !!item.blockedReason,
              firstRequestStartedMs,
              fromCache: item.status === "304" || item.fromCache,
              connector,
              columns,
              item,
              index,
              isSelected: item.id === (selectedRequest && selectedRequest.id),
              key: item.id,
              onContextMenu: this.onContextMenu,
              onFocusedNodeChange: this.onFocusedNodeChange,
              onDoubleClick: () => this.onDoubleClick(item),
              onMouseDown: evt =>
                this.onMouseDown(evt, item.id, item.channelId),
              onCauseBadgeMouseDown: () => onCauseBadgeMouseDown(item.cause),
              onSecurityIconMouseDown: () =>
                onSecurityIconMouseDown(item.securityState),
              onWaterfallMouseDown: () => onWaterfallMouseDown(),
              requestFilterTypes,
            })
          )
        ) // end of requests-list-row-group">
      )
    );
  }
}

module.exports = connect(
  state => ({
    columns: state.ui.columns,
    networkDetailsOpen: state.ui.networkDetailsOpen,
    networkDetailsWidth: state.ui.networkDetailsWidth,
    networkDetailsHeight: state.ui.networkDetailsHeight,
    clickedRequest: state.requests.clickedRequest,
    displayedRequests: getDisplayedRequests(state),
    firstRequestStartedMs: state.requests.firstStartedMs,
    selectedRequest: getSelectedRequest(state),
    scale: getWaterfallScale(state),
    requestFilterTypes: state.filters.requestFilterTypes,
    panelOpen: state.search.panelOpen,
  }),
  (dispatch, props) => ({
    blockSelectedRequestURL: clickedRequest => {
      dispatch(
        Actions.blockSelectedRequestURL(props.connector, clickedRequest)
      );
    },
    cloneRequest: id => dispatch(Actions.cloneRequest(id)),
    openDetailsPanelTab: () => dispatch(Actions.openNetworkDetails(true)),
    sendCustomRequest: () =>
      dispatch(Actions.sendCustomRequest(props.connector)),
    openStatistics: open =>
      dispatch(Actions.openStatistics(props.connector, open)),
    unblockSelectedRequestURL: clickedRequest => {
      dispatch(
        Actions.unblockSelectedRequestURL(props.connector, clickedRequest)
      );
    },
    /**
     * A handler that opens the stack trace tab when a stack trace is available
     */
    onCauseBadgeMouseDown: cause => {
      if (cause.stacktrace && cause.stacktrace.length > 0) {
        dispatch(Actions.selectDetailsPanelTab("stack-trace"));
      }
    },
    selectRequest: (id, channelId) =>
      dispatch(Actions.selectRequest(id, channelId)),
    onItemRightMouseButtonDown: id => dispatch(Actions.rightClickRequest(id)),
    onItemMouseDown: id => dispatch(Actions.selectRequest(id)),
    /**
     * A handler that opens the security tab in the details view if secure or
     * broken security indicator is clicked.
     */
    onSecurityIconMouseDown: securityState => {
      if (securityState && securityState !== "insecure") {
        dispatch(Actions.selectDetailsPanelTab("security"));
      }
    },
    onSelectDelta: delta => dispatch(Actions.selectDelta(delta)),
    /**
     * A handler that opens the timing sidebar panel if the waterfall is clicked.
     */
    onWaterfallMouseDown: () => {
      dispatch(Actions.selectDetailsPanelTab("timings"));
    },
    toggleSearchPanel: () => dispatch(Actions.toggleSearchPanel()),
  })
)(RequestListContent);
