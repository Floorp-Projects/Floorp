/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  REPS,
  MODE,
} = require("resource://devtools/client/shared/components/reps/index.js");
const { Rep } = REPS;
const TreeViewClass = require("resource://devtools/client/shared/components/tree/TreeView.js");
const TreeView = createFactory(TreeViewClass);

/**
 * The ExpressionResultView React Component is used in the ExtensionSidebar component to
 * provide a UI viewMode which shows a tree view of the passed JavaScript object.
 */
class ExpressionResultView extends PureComponent {
  static get propTypes() {
    return {
      object: PropTypes.object.isRequired,
    };
  }

  render() {
    const { object } = this.props;

    const columns = [
      {
        id: "value",
      },
    ];

    // Render the node value (omitted on the root element if it has children).
    const renderValue = props => {
      if (props.member.level === 0 && props.member.hasChildren) {
        return undefined;
      }

      return Rep(
        Object.assign({}, props, {
          cropLimit: 50,
        })
      );
    };

    return TreeView({
      object,
      mode: MODE.SHORT,
      columns,
      renderValue,
      expandedNodes: TreeViewClass.getExpandedNodes(object, {
        maxLevel: 1,
        maxNodes: 1,
      }),
    });
  }
}

module.exports = ExpressionResultView;
