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

  formatPercent(percent) {
    return L10N.getFormatStr("tree-item.percent", Math.round(percent));
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
    } = this.props;

    const percentBytes = this.formatPercent(getPercentBytes(item.bytes));
    const percentCount = this.formatPercent(getPercentCount(item.count));
    const percentTotalBytes = this.formatPercent(getPercentBytes(item.totalBytes));
    const percentTotalCount = this.formatPercent(getPercentBytes(item.totalCount));

    return dom.div({ className: `heap-tree-item ${focused ? "focused" :""}` },
      dom.span({ className: "heap-tree-item-field heap-tree-item-bytes" },
               dom.span({ className: "heap-tree-number" }, item.bytes),
               dom.span({ className: "heap-tree-percent" }, percentBytes)),
      dom.span({ className: "heap-tree-item-field heap-tree-item-count" },
               dom.span({ className: "heap-tree-number" }, item.count),
               dom.span({ className: "heap-tree-percent" }, percentCount)),
      dom.span({ className: "heap-tree-item-field heap-tree-item-total-bytes" },
               dom.span({ className: "heap-tree-number" }, item.totalBytes),
               dom.span({ className: "heap-tree-percent" }, percentTotalBytes)),
      dom.span({ className: "heap-tree-item-field heap-tree-item-total-count" },
               dom.span({ className: "heap-tree-number" }, item.totalCount),
               dom.span({ className: "heap-tree-percent" }, percentTotalCount)),
      dom.span({ className: "heap-tree-item-field heap-tree-item-name", style: { marginLeft: depth * INDENT }},
        arrow,
        this.toLabel(item.name, toolbox)
      )
    );
  },

  toLabel(name, toolbox) {
    return isSavedFrame(name) ? FrameView({ frame: name, toolbox }) :
           name === "noStack" ? L10N.getStr("tree-item.nostack") :
           name === null ? L10N.getStr("tree-item.root") :
           String(name);
  },
});
