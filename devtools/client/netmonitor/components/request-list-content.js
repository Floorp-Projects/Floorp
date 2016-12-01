/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals NetMonitorView */

"use strict";

const { Task } = require("devtools/shared/task");
const { createClass, createFactory, DOM } = require("devtools/client/shared/vendor/react");
const { div } = DOM;
const Actions = require("../actions/index");
const RequestListItem = createFactory(require("./request-list-item"));
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { setTooltipImageContent,
        setTooltipStackTraceContent } = require("./request-list-tooltip");
const { getDisplayedRequests,
        getWaterfallScale } = require("../selectors/index");
const { KeyCodes } = require("devtools/client/shared/keycodes");

// tooltip show/hide delay in ms
const REQUESTS_TOOLTIP_TOGGLE_DELAY = 500;

/**
 * Renders the actual contents of the request list.
 */
const RequestListContent = createClass({
  displayName: "RequestListContent",

  componentDidMount() {
    // Set the CSS variables for waterfall scaling
    this.setScalingStyles();

    // Install event handler for displaying a tooltip
    this.props.tooltip.startTogglingOnHover(this.refs.contentEl, this.onHover, {
      toggleDelay: REQUESTS_TOOLTIP_TOGGLE_DELAY,
      interactive: true
    });

    // Install event handler to hide the tooltip on scroll
    this.refs.contentEl.addEventListener("scroll", this.onScroll, true);
  },

  componentWillUpdate() {
    // Check if the list is scrolled to bottom, before UI update
    this.shouldScrollBottom = this.isScrolledToBottom();
  },

  componentDidUpdate(prevProps) {
    // Update the CSS variables for waterfall scaling after props change
    this.setScalingStyles();

    // Keep the list scrolled to bottom if a new row was added
    if (this.shouldScrollBottom) {
      let node = this.refs.contentEl;
      node.scrollTop = node.scrollHeight;
    }
  },

  componentWillUnmount() {
    this.refs.contentEl.removeEventListener("scroll", this.onScroll, true);

    // Uninstall the tooltip event handler
    this.props.tooltip.stopTogglingOnHover();
  },

  /**
   * Set the CSS variables for waterfall scaling. If React supported setting CSS
   * variables as part of the "style" property of a DOM element, we would use that.
   *
   * However, React doesn't support this, so we need to use a hack and update the
   * DOM element directly: https://github.com/facebook/react/issues/6411
   */
  setScalingStyles(prevProps) {
    const { scale } = this.props;
    if (scale == this.currentScale) {
      return;
    }

    this.currentScale = scale;

    const { style } = this.refs.contentEl;
    style.removeProperty("--timings-scale");
    style.removeProperty("--timings-rev-scale");
    style.setProperty("--timings-scale", scale);
    style.setProperty("--timings-rev-scale", 1 / scale);
  },

  isScrolledToBottom() {
    const { contentEl } = this.refs;
    const lastChildEl = contentEl.lastElementChild;

    if (!lastChildEl) {
      return false;
    }

    let lastChildRect = lastChildEl.getBoundingClientRect();
    let contentRect = contentEl.getBoundingClientRect();

    return (lastChildRect.height + lastChildRect.top) <= contentRect.bottom;
  },

  /**
   * The predicate used when deciding whether a popup should be shown
   * over a request item or not.
   *
   * @param nsIDOMNode target
   *        The element node currently being hovered.
   * @param object tooltip
   *        The current tooltip instance.
   * @return {Promise}
   */
  onHover: Task.async(function* (target, tooltip) {
    let itemEl = target.closest(".request-list-item");
    if (!itemEl) {
      return false;
    }
    let itemId = itemEl.dataset.id;
    if (!itemId) {
      return false;
    }
    let requestItem = this.props.displayedRequests.find(r => r.id == itemId);
    if (!requestItem) {
      return false;
    }

    if (requestItem.responseContent && target.closest(".requests-menu-icon-and-file")) {
      return setTooltipImageContent(tooltip, itemEl, requestItem);
    } else if (requestItem.cause && target.closest(".requests-menu-cause-stack")) {
      return setTooltipStackTraceContent(tooltip, requestItem);
    }

    return false;
  }),

  /**
   * Scroll listener for the requests menu view.
   */
  onScroll() {
    this.props.tooltip.hide();
  },

  /**
   * Handler for keyboard events. For arrow up/down, page up/down, home/end,
   * move the selection up or down.
   */
  onKeyDown(e) {
    let delta;

    switch (e.keyCode) {
      case KeyCodes.DOM_VK_UP:
      case KeyCodes.DOM_VK_LEFT:
        delta = -1;
        break;
      case KeyCodes.DOM_VK_DOWN:
      case KeyCodes.DOM_VK_RIGHT:
        delta = +1;
        break;
      case KeyCodes.DOM_VK_PAGE_UP:
        delta = "PAGE_UP";
        break;
      case KeyCodes.DOM_VK_PAGE_DOWN:
        delta = "PAGE_DOWN";
        break;
      case KeyCodes.DOM_VK_HOME:
        delta = -Infinity;
        break;
      case KeyCodes.DOM_VK_END:
        delta = +Infinity;
        break;
    }

    if (delta) {
      // Prevent scrolling when pressing navigation keys.
      e.preventDefault();
      e.stopPropagation();
      this.props.onSelectDelta(delta);
    }
  },

  /**
   * If selection has just changed (by keyboard navigation), don't keep the list
   * scrolled to bottom, but allow scrolling up with the selection.
   */
  onFocusedNodeChange() {
    this.shouldScrollBottom = false;
  },

  /**
   * If a focused item was unmounted, transfer the focus to the container element.
   */
  onFocusedNodeUnmount() {
    if (this.refs.contentEl) {
      this.refs.contentEl.focus();
    }
  },

  render() {
    const { selectedRequestId,
            displayedRequests,
            firstRequestStartedMillis,
            onItemMouseDown,
            onItemContextMenu,
            onSecurityIconClick } = this.props;

    return div(
      {
        ref: "contentEl",
        className: "requests-menu-contents",
        tabIndex: 0,
        onKeyDown: this.onKeyDown,
      },
      displayedRequests.map((item, index) => RequestListItem({
        key: item.id,
        item,
        index,
        isSelected: item.id === selectedRequestId,
        firstRequestStartedMillis,
        onMouseDown: e => onItemMouseDown(e, item.id),
        onContextMenu: e => onItemContextMenu(e, item.id),
        onSecurityIconClick: e => onSecurityIconClick(e, item),
        onFocusedNodeChange: this.onFocusedNodeChange,
        onFocusedNodeUnmount: this.onFocusedNodeUnmount,
      }))
    );
  },
});

module.exports = connect(
  state => ({
    displayedRequests: getDisplayedRequests(state),
    selectedRequestId: state.requests.selectedId,
    scale: getWaterfallScale(state),
    firstRequestStartedMillis: state.requests.firstStartedMillis,
    tooltip: NetMonitorView.RequestsMenu.tooltip,
  }),
  dispatch => ({
    onItemMouseDown: (e, item) => dispatch(Actions.selectRequest(item)),
    onItemContextMenu: (e, item) => {
      e.preventDefault();
      NetMonitorView.RequestsMenu.contextMenu.open(e);
    },
    onSelectDelta: (delta) => dispatch(Actions.selectDelta(delta)),
    /**
     * A handler that opens the security tab in the details view if secure or
     * broken security indicator is clicked.
     */
    onSecurityIconClick: (e, item) => {
      const { securityState } = item;
      if (securityState && securityState !== "insecure") {
        // Choose the security tab.
        NetMonitorView.NetworkDetails.widget.selectedIndex = 5;
      }
    },
  })
)(RequestListContent);
