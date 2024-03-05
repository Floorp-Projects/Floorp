/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const Tree = createFactory(
  require("resource://devtools/client/shared/components/VirtualizedTree.js")
);
const DominatorTreeItem = createFactory(
  require("resource://devtools/client/memory/components/DominatorTreeItem.js")
);
const {
  TREE_ROW_HEIGHT,
} = require("resource://devtools/client/memory/constants.js");
const models = require("resource://devtools/client/memory/models.js");

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
    const { individuals, dominatorTree, onViewSourceInDebugger, onFocus } =
      this.props;

    return Tree({
      key: "individuals-tree",
      autoExpandDepth: 0,
      preventNavigationOnArrowRight: false,
      focused: individuals.focused,
      getParent: () => null,
      getChildren: () => [],
      isExpanded: () => false,
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
          getPercentSize: size =>
            (size / dominatorTree.root.retainedSize) * 100,
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
