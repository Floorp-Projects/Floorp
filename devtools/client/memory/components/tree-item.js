/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { isSavedFrame } = require("devtools/shared/DevToolsUtils");
const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils");
const FrameView = createFactory(require("./frame"));

const INDENT = 10;
const MAX_SOURCE_LENGTH = 200;


/**
 * An arrow that displays whether its node is expanded (▼) or collapsed
 * (▶). When its node has no children, it is hidden.
 */
const TreeItem = module.exports = createClass({
  displayName: "tree-item",

  formatPercent(showSign, percent) {
    return L10N.getFormatStr("tree-item.percent",
                             this.formatNumber(showSign, percent));
  },

  formatNumber(showSign, number) {
    const rounded = Math.round(number);
    if (rounded === 0 || rounded === -0) {
      return "0";
    }

    let sign = "";
    if (showSign) {
      sign = rounded < 0 ? "-" : "+";
    }

    return sign + Math.abs(rounded);
  },

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.item != nextProps.item
      || this.props.depth != nextProps.depth
      || this.props.expanded != nextProps.expanded
      || this.props.focused != nextProps.focused
      || this.props.showSign != nextProps.showSign;
  },

  render() {
    let {
      item,
      depth,
      arrow,
      focused,
      toolbox,
      getPercentBytes,
      getPercentCount,
      showSign,
    } = this.props;

    const bytes = this.formatNumber(showSign, item.bytes);
    const percentBytes = this.formatPercent(showSign, getPercentBytes(item.bytes));

    const count = this.formatNumber(showSign, item.count);
    const percentCount = this.formatPercent(showSign, getPercentCount(item.count));

    const totalBytes = this.formatNumber(showSign, item.totalBytes);
    const percentTotalBytes = this.formatPercent(showSign, getPercentBytes(item.totalBytes));

    const totalCount = this.formatNumber(showSign, item.totalCount);
    const percentTotalCount = this.formatPercent(showSign, getPercentCount(item.totalCount));

    return dom.div({ className: `heap-tree-item ${focused ? "focused" :""}` },
      dom.span({ className: "heap-tree-item-field heap-tree-item-bytes" },
               dom.span({ className: "heap-tree-number" }, bytes),
               dom.span({ className: "heap-tree-percent" }, percentBytes)),
      dom.span({ className: "heap-tree-item-field heap-tree-item-count" },
               dom.span({ className: "heap-tree-number" }, count),
               dom.span({ className: "heap-tree-percent" }, percentCount)),
      dom.span({ className: "heap-tree-item-field heap-tree-item-total-bytes" },
               dom.span({ className: "heap-tree-number" }, totalBytes),
               dom.span({ className: "heap-tree-percent" }, percentTotalBytes)),
      dom.span({ className: "heap-tree-item-field heap-tree-item-total-count" },
               dom.span({ className: "heap-tree-number" }, totalCount),
               dom.span({ className: "heap-tree-percent" }, percentTotalCount)),
      dom.span({ className: "heap-tree-item-field heap-tree-item-name", style: { marginLeft: depth * INDENT }},
        arrow,
        this.toLabel(item.name, toolbox)
      )
    );
  },

  toLabel(name, toolbox) {
    if (isSavedFrame(name)) {
      return FrameView({ frame: name, toolbox });
    }

    if (name === null) {
      return L10N.getStr("tree-item.root");
    }

    if (name === "noStack") {
      return L10N.getStr("tree-item.nostack");
    }

    if (name === "noFilename") {
      return L10N.getStr("tree-item.nofilename");
    }

    return String(name);
  },
});
