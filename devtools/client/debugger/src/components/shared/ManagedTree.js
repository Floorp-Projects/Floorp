/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import "./ManagedTree.css";

const { Tree } = require("devtools-components");

export type Item = {
  contents: any,
  name: string,
  path: string,
};

type Props = {
  autoExpandAll: boolean,
  autoExpandDepth: number,
  getChildren: Object => Object[],
  getPath: (Object, index?: number) => string,
  getParent: Item => any,
  getRoots: () => any,
  highlightItems?: Array<Item>,
  itemHeight: number,
  listItems?: Array<Item>,
  onFocus: (item: any) => void,
  onExpand?: (item: Item, expanded: Set<string>) => void,
  onCollapse?: (item: Item, expanded: Set<string>) => void,
  renderItem: any,
  focused?: any,
  expanded?: any,
};

type State = {
  expanded: any,
};

class ManagedTree extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = {
      expanded: props.expanded || new Set(),
    };
  }

  static defaultProps = {
    onFocus: () => {},
  };

  componentWillReceiveProps(nextProps: Props) {
    const { listItems, highlightItems } = this.props;
    if (nextProps.listItems && nextProps.listItems != listItems) {
      this.expandListItems(nextProps.listItems);
    }

    if (
      nextProps.highlightItems &&
      nextProps.highlightItems != highlightItems &&
      nextProps.highlightItems.length
    ) {
      this.highlightItem(nextProps.highlightItems);
    }
  }

  setExpanded = (
    item: Item,
    isExpanded: boolean,
    shouldIncludeChildren: boolean
  ) => {
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
          if (parent.contents && parent.contents.length) {
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

  expandListItems(listItems: Array<Item>) {
    const { expanded } = this.state;
    listItems.forEach(item => expanded.add(this.props.getPath(item)));
    this.props.onFocus(listItems[0]);
    this.setState({ expanded });
  }

  highlightItem(highlightItems: Array<Item>) {
    const { expanded } = this.state;
    // This file is visible, so we highlight it.
    if (expanded.has(this.props.getPath(highlightItems[0]))) {
      this.props.onFocus(highlightItems[0]);
    } else {
      // Look at folders starting from the top-level until finds a
      // closed folder and highlights this folder
      const index = highlightItems
        .reverse()
        .findIndex(
          item =>
            !expanded.has(this.props.getPath(item)) && item.name !== "root"
        );

      if (highlightItems[index]) {
        this.props.onFocus(highlightItems[index]);
      }
    }
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
