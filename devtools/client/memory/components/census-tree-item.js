/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { isSavedFrame } = require("devtools/shared/DevToolsUtils");
const { DOM: dom, createClass, createFactory } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils");
const Frame = createFactory(require("devtools/client/shared/components/frame"));
const unknownSourceString = L10N.getStr("unknownSource");
const { TREE_ROW_HEIGHT } = require("../constants");

const CensusTreeItem = module.exports = createClass({
  displayName: "CensusTreeItem",

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
      onViewSourceInDebugger,
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
               dom.span({ className: "heap-tree-item-field heap-tree-item-name",
                          style: { marginLeft: depth * TREE_ROW_HEIGHT }},
        arrow,
        this.toLabel(item.name, onViewSourceInDebugger)
      )
    );
  },

  toLabel(name, linkToDebugger) {
    if (isSavedFrame(name)) {
      let onClickTooltipString =
        L10N.getFormatStr("viewsourceindebugger",`${name.source}:${name.line}:${name.column}`);
      return Frame({
        frame: name,
        onClick: () => linkToDebugger(name),
        onClickTooltipString,
        unknownSourceString
      });
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
