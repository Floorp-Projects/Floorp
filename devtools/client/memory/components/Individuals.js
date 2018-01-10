/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Tree = createFactory(require("devtools/client/shared/components/VirtualizedTree"));
const DominatorTreeItem = createFactory(require("./DominatorTreeItem"));
const { TREE_ROW_HEIGHT } = require("../constants");
const models = require("../models");

/**
 * The list of individuals in a census group.
 */
class Individuals extends Component {
  static get propTypes() {
    return {
      onViewSourceInDebugger: PropTypes.func.isRequired,
      onFocus: PropTypes.func.isRequired,
      individuals: models.individuals,
      dominatorTree: models.dominatorTreeModel,
    };
  }

  render() {
    const {
      individuals,
      dominatorTree,
      onViewSourceInDebugger,
      onFocus,
    } = this.props;

    return Tree({
      key: "individuals-tree",
      autoExpandDepth: 0,
      focused: individuals.focused,
      getParent: node => null,
      getChildren: node => [],
      isExpanded: node => false,
      onExpand: () => {},
      onCollapse: () => {},
      onFocus,
      renderItem: (item, depth, focused, _, expanded) => {
        return DominatorTreeItem({
          item,
          depth,
          focused,
          arrow: undefined,
          expanded,
          getPercentSize: size => (size / dominatorTree.root.retainedSize) * 100,
          onViewSourceInDebugger,
        });
      },
      getRoots: () => individuals.nodes,
      getKey: node => node.nodeId,
      itemHeight: TREE_ROW_HEIGHT,
    });
  }
}

module.exports = Individuals;
