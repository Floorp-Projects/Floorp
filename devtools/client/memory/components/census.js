/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes, createFactory } = require("devtools/client/shared/vendor/react");
const Tree = createFactory(require("devtools/client/shared/components/tree"));
const CensusTreeItem = createFactory(require("./census-tree-item"));
const { createParentMap } = require("../utils");
const { TREE_ROW_HEIGHT } = require("../constants");
const { censusModel, diffingModel } = require("../models");

const Census = module.exports = createClass({
  displayName: "Census",

  propTypes: {
    census: censusModel,
    onExpand: PropTypes.func.isRequired,
    onCollapse: PropTypes.func.isRequired,
    onFocus: PropTypes.func.isRequired,
    onViewSourceInDebugger: PropTypes.func.isRequired,
    diffing: diffingModel,
  },

  render() {
    let {
      census,
      onExpand,
      onCollapse,
      onFocus,
      diffing,
      onViewSourceInDebugger,
    } = this.props;

    const report = census.report;
    let parentMap = createParentMap(report);
    const { totalBytes, totalCount } = report;

    const getPercentBytes = totalBytes === 0
      ? _ => 0
      : bytes => (bytes / totalBytes) * 100;

    const getPercentCount = totalCount === 0
      ? _ => 0
      : count => (count / totalCount) * 100;

    return Tree({
      autoExpandDepth: 0,
      focused: census.focused,
      getParent: node => {
        const parent = parentMap[node.id];
        return parent === report ? null : parent;
      },
      getChildren: node => node.children || [],
      isExpanded: node => census.expanded.has(node.id),
      onExpand,
      onCollapse,
      onFocus,
      renderItem: (item, depth, focused, arrow, expanded) =>
        new CensusTreeItem({
          onViewSourceInDebugger,
          item,
          depth,
          focused,
          arrow,
          expanded,
          getPercentBytes,
          getPercentCount,
          showSign: !!diffing,
        }),
      getRoots: () => report.children || [],
      getKey: node => node.id,
      itemHeight: TREE_ROW_HEIGHT,
    });
  }
});
