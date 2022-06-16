/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import "./ManagedTree.css";

const Tree = require("devtools/client/shared/components/Tree");

class ManagedTree extends Component {
  constructor(props) {
    super(props);
    this.state = {
      expanded: props.expanded || new Set(),
    };
  }

  static defaultProps = {
    onFocus: () => {},
  };

  static get propTypes() {
    return {
      expanded: PropTypes.object,
      focused: PropTypes.any,
      getPath: PropTypes.func.isRequired,
      highlightItems: PropTypes.array,
      onCollapse: PropTypes.func.isRequired,
      onExpand: PropTypes.func.isRequired,
      onFocus: PropTypes.func.isRequired,
      renderItem: PropTypes.func.isRequired,
    };
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { highlightItems } = this.props;
    if (
      nextProps.highlightItems &&
      nextProps.highlightItems != highlightItems &&
      nextProps.highlightItems.length
    ) {
      this.highlightItem(nextProps.highlightItems);
    }
  }

  setExpanded = (item, isExpanded, shouldIncludeChildren) => {
    const expandItem = i => {
      const path = this.props.getPath(i);
      if (isExpanded) {
        expanded.add(path);
      } else {
        expanded.delete(path);
      }
    };
    const { expanded } = this.state;
    expandItem(item);

    if (shouldIncludeChildren) {
      let parents = [item];
      while (parents.length) {
        const children = [];
        for (const parent of parents) {
          if (parent.contents?.length) {
            for (const child of parent.contents) {
              expandItem(child);
              children.push(child);
            }
          }
        }
        parents = children;
      }
    }
    this.setState({ expanded });

    if (isExpanded && this.props.onExpand) {
      this.props.onExpand(item, expanded);
    } else if (!isExpanded && this.props.onCollapse) {
      this.props.onCollapse(item, expanded);
    }
  };

  highlightItem(highlightItems) {
    const { expanded } = this.state;

    highlightItems.forEach(item => {
      expanded.add(this.props.getPath(item));
    });
    this.props.onFocus(highlightItems[0]);
    this.setState({ expanded });
  }

  render() {
    const { expanded } = this.state;
    return (
      <div className="managed-tree">
        <Tree
          {...this.props}
          isExpanded={item => expanded.has(this.props.getPath(item))}
          focused={this.props.focused}
          getKey={this.props.getPath}
          onExpand={(item, shouldIncludeChildren) =>
            this.setExpanded(item, true, shouldIncludeChildren)
          }
          onCollapse={(item, shouldIncludeChildren) =>
            this.setExpanded(item, false, shouldIncludeChildren)
          }
          onFocus={this.props.onFocus}
          renderItem={(...args) =>
            this.props.renderItem(...args, {
              setExpanded: this.setExpanded,
            })
          }
        />
      </div>
    );
  }
}

export default ManagedTree;
