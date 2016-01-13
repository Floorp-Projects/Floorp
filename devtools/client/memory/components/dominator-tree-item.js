/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { assert, isSavedFrame } = require("devtools/shared/DevToolsUtils");
const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils");
const Frame = createFactory(require("devtools/client/shared/components/frame"));
const { TREE_ROW_HEIGHT } = require("../constants");

const Separator = createFactory(createClass({
  displayName: "Separator",

  render() {
    return dom.span({ className: "separator" }, "›");
  }
}));

const DominatorTreeItem = module.exports = createClass({
  displayName: "DominatorTreeItem",

  propTypes: {
    item: PropTypes.object.isRequired,
    depth: PropTypes.number.isRequired,
    arrow: PropTypes.object.isRequired,
    focused: PropTypes.bool.isRequired,
    getPercentSize: PropTypes.func.isRequired,
    onViewSourceInDebugger: PropTypes.func.isRequired,
  },

  formatPercent(percent) {
    return L10N.getFormatStr("tree-item.percent",
                             this.formatNumber(percent));
  },

  formatNumber(number) {
    const rounded = Math.round(number);
    if (rounded === 0 || rounded === -0) {
      return "0";
    }

    return String(Math.abs(rounded));
  },

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.item != nextProps.item
      || this.props.depth != nextProps.depth
      || this.props.focused != nextProps.focused;
  },

  render() {
    let {
      item,
      depth,
      arrow,
      focused,
      getPercentSize,
      onViewSourceInDebugger,
    } = this.props;

    const retainedSize = this.formatNumber(item.retainedSize);
    const percentRetainedSize = this.formatPercent(getPercentSize(item.retainedSize));

    const shallowSize = this.formatNumber(item.shallowSize);
    const percentShallowSize = this.formatPercent(getPercentSize(item.shallowSize));

    // Build up our label UI as an array of each label piece, which is either a
    // string or a frame, and separators in between them.

    assert(item.label.length > 0,
           "Our label should not be empty");
    const label = Array(item.label.length * 2 - 1);
    label.fill(undefined);

    for (let i = 0, length = item.label.length; i < length; i++) {
      const piece = item.label[i];
      const key = `${item.nodeId}-label-${i}`;

      // `i` is the index of the label piece we are rendering, `label[i*2]` is
      // where the rendered label piece belngs, and `label[i*2+1]` (if it isn't
      // out of bounds) is where the separator belongs.

      if (isSavedFrame(piece)) {
        label[i * 2] = Frame({
          key,
          onClick: () => onViewSourceInDebugger(piece),
          frame: piece
        });
      } else if (piece === "noStack") {
        label[i * 2] = dom.span({ key, className: "not-available" },
                                L10N.getStr("tree-item.nostack"));
      } else if (piece === "noFilename") {
        label[i * 2] = dom.span({ key, className: "not-available" },
                                L10N.getStr("tree-item.nofilename"));
      } else {
        label[i * 2] = piece;
      }

      // If this is not the last piece of the label, add a separator.
      if (i < length - 1) {
        label[i * 2 + 1] = Separator({ key: `${item.nodeId}-separator-${i}` });
      }
    }

    return dom.div(
      {
        className: `heap-tree-item ${focused ? "focused" : ""} node-${item.nodeId}`
      },

      dom.span(
        {
          className: "heap-tree-item-field heap-tree-item-bytes"
        },
        dom.span(
          {
            className: "heap-tree-number"
          },
          retainedSize
        ),
        dom.span({ className: "heap-tree-percent" }, percentRetainedSize)
      ),

      dom.span(
        {
          className: "heap-tree-item-field heap-tree-item-bytes"
        },
        dom.span(
          {
            className: "heap-tree-number"
          },
          shallowSize
        ),
        dom.span({ className: "heap-tree-percent" }, percentShallowSize)
      ),

      dom.span(
        {
          className: "heap-tree-item-field heap-tree-item-name",
          style: { marginLeft: depth * TREE_ROW_HEIGHT }
        },
        arrow,
        label,
        dom.span({ className: "heap-tree-item-address" },
                 `@ 0x${item.nodeId.toString(16)}`)
      )
    );
  },
});
