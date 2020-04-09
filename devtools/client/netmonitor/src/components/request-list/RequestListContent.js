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

const Actions = require("devtools/client/netmonitor/src/actions/index");
const {
  formDataURI,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  getDisplayedRequests,
  getColumns,
  getSelectedRequest,
} = require("devtools/client/netmonitor/src/selectors/index");

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
const RequestListHeader = createFactory(
  require("devtools/client/netmonitor/src/components/request-list/RequestListHeader")
);
const RequestListItem = createFactory(
  require("devtools/client/netmonitor/src/components/request-list/RequestListItem")
);
const RequestListContextMenu = require("devtools/client/netmonitor/src/widgets/RequestListContextMenu");

const { div } = dom;

// Tooltip show / hide delay in ms
const REQUESTS_TOOLTIP_TOGGLE_DELAY = 500;
// Tooltip image maximum dimension in px
const REQUESTS_TOOLTIP_IMAGE_MAX_DIM = 400;

const LEFT_MOUSE_BUTTON = 0;
const RIGHT_MOUSE_BUTTON = 2;

/**
 * Renders the actual contents of the request list.
 */
class RequestListContent extends Component {
  static get propTypes() {
    return {
      blockedUrls: PropTypes.array.isRequired,
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
      onInitiatorBadgeMouseDown: PropTypes.func.isRequired,
      onItemRightMouseButtonDown: PropTypes.func.isRequired,
      onItemMouseDown: PropTypes.func.isRequired,
      onSecurityIconMouseDown: PropTypes.func.isRequired,
      onSelectDelta: PropTypes.func.isRequired,
      onWaterfallMouseDown: PropTypes.func.isRequired,
      openStatistics: PropTypes.func.isRequired,
      openRequestBlockingAndAddUrl: PropTypes.func.isRequired,
      openRequestBlockingAndDisableUrls: PropTypes.func.isRequired,
      removeBlockedUrl: PropTypes.func.isRequired,
      selectRequest: PropTypes.func.isRequired,
      selectedRequest: PropTypes.object,
      requestFilterTypes: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onHover = this.onHover.bind(this);
    this.onScroll = this.onScroll.bind(this);
    this.onResize = this.onResize.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.openRequestInTab = this.openRequestInTab.bind(this);
    this.onDoubleClick = this.onDoubleClick.bind(this);
    this.onContextMenu = this.onContextMenu.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
    this.hasOverflow = false;
    this.onIntersect = this.onIntersect.bind(this);
    this.intersectionObserver = null;
    this.state = {
      onscreenItems: new Set(),
    };
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
    this.intersectionObserver = new IntersectionObserver(this.onIntersect, {
      root: this.refs.scrollEl,
      // Render 10% more columns for a scrolling headstart
      rootMargin: "10%",
    });
    // Prime IntersectionObserver with existing entries
    for (const item of this.refs.scrollEl.querySelectorAll(
      ".request-list-item"
    )) {
      this.intersectionObserver.observe(item);
    }
  }

  componentDidUpdate(prevProps) {
    const output = this.refs.scrollEl;
    if (!this.hasOverflow && output.scrollHeight > output.clientHeight) {
      output.scrollTop = output.scrollHeight;
      this.hasOverflow = true;
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
    this.intersectionObserver.disconnect();
    this.intersectionObserver = null;
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

  onIntersect(entries) {
    // Track when off screen elements moved on screen to ensure updates
    let onscreenDidChange = false;
    const onscreenItems = new Set(this.state.onscreenItems);
    for (const { target, isIntersecting } of entries) {
      const id = target.dataset.id;
      if (isIntersecting) {
        if (onscreenItems.add(id)) {
          onscreenDidChange = true;
        }
      } else {
        onscreenItems.delete(id);
      }
    }
    if (onscreenDidChange) {
      // Remove ids that are no longer displayed
      const itemIds = new Set(this.props.displayedRequests.map(({ id }) => id));
      for (const id of onscreenItems) {
        if (!itemIds.has(id)) {
          onscreenItems.delete(id);
        }
      }
      this.setState({ onscreenItems });
    }
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
        delta = -1;
        break;
      case "ArrowDown":
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
    const { clickedRequest, displayedRequests, blockedUrls } = this.props;

    if (!this.contextMenu) {
      const {
        connector,
        cloneRequest,
        openDetailsPanelTab,
        sendCustomRequest,
        openStatistics,
        openRequestBlockingAndAddUrl,
        openRequestBlockingAndDisableUrls,
        removeBlockedUrl,
      } = this.props;
      this.contextMenu = new RequestListContextMenu({
        connector,
        cloneRequest,
        openDetailsPanelTab,
        sendCustomRequest,
        openStatistics,
        openRequestBlockingAndAddUrl,
        openRequestBlockingAndDisableUrls,
        removeBlockedUrl,
        openRequestInTab: this.openRequestInTab,
      });
    }

    this.contextMenu.open(evt, clickedRequest, displayedRequests, blockedUrls);
  }

  render() {
    const {
      connector,
      columns,
      displayedRequests,
      firstRequestStartedMs,
      onInitiatorBadgeMouseDown,
      onSecurityIconMouseDown,
      onWaterfallMouseDown,
      requestFilterTypes,
      selectedRequest,
      openRequestBlockingAndAddUrl,
      openRequestBlockingAndDisableUrls,
      networkDetailsOpen,
    } = this.props;

    return div(
      {
        ref: "scrollEl",
        className: "requests-list-scroll",
      },
      [
        dom.table(
          {
            className: "requests-list-table",
            key: "table",
          },
          RequestListHeader(),
          dom.tbody(
            {
              ref: "rowGroupEl",
              className: "requests-list-row-group",
              tabIndex: 0,
              onKeyDown: this.onKeyDown,
            },
            displayedRequests.map((item, index) => {
              return RequestListItem({
                blocked: !!item.blockedReason,
                firstRequestStartedMs,
                fromCache: item.status === "304" || item.fromCache,
                networkDetailsOpen,
                connector,
                columns,
                item,
                index,
                isSelected: item.id === selectedRequest?.id,
                isVisible: this.state.onscreenItems.has(item.id),
                key: item.id,
                intersectionObserver: this.intersectionObserver,
                onContextMenu: this.onContextMenu,
                onDoubleClick: () => this.onDoubleClick(item),
                onMouseDown: evt =>
                  this.onMouseDown(evt, item.id, item.channelId),
                onInitiatorBadgeMouseDown: () =>
                  onInitiatorBadgeMouseDown(item.cause),
                onSecurityIconMouseDown: () =>
                  onSecurityIconMouseDown(item.securityState),
                onWaterfallMouseDown: onWaterfallMouseDown,
                requestFilterTypes,
                openRequestBlockingAndAddUrl: openRequestBlockingAndAddUrl,
                openRequestBlockingAndDisableUrls: openRequestBlockingAndDisableUrls,
              });
            })
          )
        ), // end of requests-list-row-group">
        dom.div({
          className: "requests-list-anchor",
          key: "anchor",
        }),
      ]
    );
  }
}

module.exports = connect(
  state => ({
    blockedUrls: state.requestBlocking.blockedUrls
      .map(({ enabled, url }) => (enabled ? url : null))
      .filter(Boolean),
    columns: getColumns(state),
    networkDetailsOpen: state.ui.networkDetailsOpen,
    networkDetailsWidth: state.ui.networkDetailsWidth,
    networkDetailsHeight: state.ui.networkDetailsHeight,
    clickedRequest: state.requests.clickedRequest,
    displayedRequests: getDisplayedRequests(state),
    firstRequestStartedMs: state.requests.firstStartedMs,
    selectedRequest: getSelectedRequest(state),
    requestFilterTypes: state.filters.requestFilterTypes,
  }),
  (dispatch, props) => ({
    cloneRequest: id => dispatch(Actions.cloneRequest(id)),
    openDetailsPanelTab: () => dispatch(Actions.openNetworkDetails(true)),
    sendCustomRequest: () =>
      dispatch(Actions.sendCustomRequest(props.connector)),
    openStatistics: open =>
      dispatch(Actions.openStatistics(props.connector, open)),
    openRequestBlockingAndAddUrl: url =>
      dispatch(Actions.openRequestBlockingAndAddUrl(url)),
    removeBlockedUrl: url => dispatch(Actions.removeBlockedUrl(url)),
    openRequestBlockingAndDisableUrls: url =>
      dispatch(Actions.openRequestBlockingAndDisableUrls(url)),
    /**
     * A handler that opens the stack trace tab when a stack trace is available
     */
    onInitiatorBadgeMouseDown: cause => {
      if (cause.lastFrame) {
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
  })
)(RequestListContent);
