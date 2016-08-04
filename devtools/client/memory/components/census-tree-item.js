/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { isSavedFrame } = require("devtools/shared/DevToolsUtils");
const { DOM: dom, createClass, createFactory } = require("devtools/client/shared/vendor/react");
const { L10N, formatNumber, formatPercent } = require("../utils");
const Frame = createFactory(require("devtools/client/shared/components/frame"));
const { TREE_ROW_HEIGHT } = require("../constants");

const CensusTreeItem = module.exports = createClass({
  displayName: "CensusTreeItem",

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.item != nextProps.item
      || this.props.depth != nextProps.depth
      || this.props.expanded != nextProps.expanded
      || this.props.focused != nextProps.focused
      || this.props.diffing != nextProps.diffing;
  },

  render() {
    let {
      item,
      depth,
      arrow,
      focused,
      getPercentBytes,
      getPercentCount,
      diffing,
      onViewSourceInDebugger,
      onViewIndividuals,
      inverted,
    } = this.props;

    const bytes = formatNumber(item.bytes, !!diffing);
    const percentBytes = formatPercent(getPercentBytes(item.bytes), !!diffing);

    const count = formatNumber(item.count, !!diffing);
    const percentCount = formatPercent(getPercentCount(item.count), !!diffing);

    const totalBytes = formatNumber(item.totalBytes, !!diffing);
    const percentTotalBytes = formatPercent(getPercentBytes(item.totalBytes), !!diffing);

    const totalCount = formatNumber(item.totalCount, !!diffing);
    const percentTotalCount = formatPercent(getPercentCount(item.totalCount), !!diffing);

    let pointer;
    if (inverted && depth > 0) {
      pointer = dom.span({ className: "children-pointer" }, "↖");
    } else if (!inverted && item.children && item.children.length) {
      pointer = dom.span({ className: "children-pointer" }, "↘");
    }

    let individualsCell;
    if (!diffing) {
      let individualsButton;
      if (item.reportLeafIndex !== undefined) {
        individualsButton = dom.button(
          {
            key: `individuals-button-${item.id}`,
            title: L10N.getStr("tree-item.view-individuals.tooltip"),
            className: "devtools-button individuals-button",
            onClick: e => {
              // Don't let the event bubble up to cause this item to focus after
              // we have switched views, which would lead to assertion failures.
              e.preventDefault();
              e.stopPropagation();

              onViewIndividuals(item);
            },
          },
          "⁂"
        );
      }
      individualsCell = dom.span(
        { className: "heap-tree-item-field heap-tree-item-individuals" },
        individualsButton
      );
    }

    return dom.div(
      { className: `heap-tree-item ${focused ? "focused" : ""}` },
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
      individualsCell,
      dom.span(
        {
          className: "heap-tree-item-field heap-tree-item-name",
          style: { marginInlineStart: depth * TREE_ROW_HEIGHT }
        },
        arrow,
        pointer,
        this.toLabel(item.name, onViewSourceInDebugger)
      )
    );
  },

  toLabel(name, linkToDebugger) {
    if (isSavedFrame(name)) {
      return Frame({
        frame: name,
        onClick: () => linkToDebugger(name),
        showFunctionName: true,
        showHost: true,
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
