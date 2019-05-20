/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
const { Component, createFactory, createElement } = React;

import Components from "../index";
const Tree = createFactory(Components.Tree);
import { storiesOf } from "@storybook/react";

storiesOf("Tree", module)
  .add("Simple tree - autoExpand 1", () => {
    return renderTree({
      autoExpandDepth: 1,
    });
  })
  .add("Simple tree - autoExpand 2", () => {
    return renderTree({
      autoExpandDepth: 2,
    });
  })
  .add("Simple tree - autoExpand ∞", () => {
    return renderTree({
      autoExpandDepth: Infinity,
    });
  })
  .add("Multiple root tree", () => {
    return renderTree({
      autoExpandDepth: Infinity,
      getRoots: () => ["A", "P", "M", "Q", "W", "R"],
    });
  })
  .add("focused node", () => {
    return renderTree({
      focused: "W",
      getRoots: () => ["A", "W"],
      expanded: new Set(["A"]),
    });
  })
  .add("variable height nodes", () => {
    const nodes = Array.from({ length: 10 }).map((_, i) =>
      `item ${i + 1} - `.repeat(10 + Math.random() * 50)
    );
    return renderTree(
      {
        getRoots: () => ["ROOT"],
        expanded: new Set(["ROOT"]),
      },
      {
        children: { ROOT: nodes },
      }
    );
  })
  .add("scrollable tree", () => {
    const nodes = Array.from({ length: 500 }).map((_, i) => (i + 1).toString());

    class container extends Component {
      constructor(props) {
        super(props);
        const state = {
          expanded: new Set(),
          focused: null,
        };
        this.state = state;
      }

      render() {
        return createElement(
          "div",
          {},
          createElement(
            "label",
            {
              style: { position: "fixed", right: 0 },
            },
            "Enter node number to set focus on: ",
            createElement("input", {
              type: "number",
              min: 1,
              max: 500,
              onInput: e => {
                // Storing balue since `e` can be reused by the time the setState
                // callback is called.
                const value = e.target.value.toString();
                this.setState(previousState => {
                  return { focused: value };
                });
              },
            })
          ),
          createTreeElement({ getRoots: () => nodes }, this, {})
        );
      }
    }
    return createFactory(container)();
  })
  .add("scrollable tree with focused node", () => {
    const nodes = Array.from({ length: 500 }).map((_, i) => `item ${i + 1}`);
    return renderTree(
      {
        focused: "item 250",
        getRoots: () => nodes,
      },
      {}
    );
  })
  .add("1000 items tree", () => {
    const nodes = Array.from({ length: 1000 }).map((_, i) => `item-${i + 1}`);
    return renderTree(
      {
        getRoots: () => ["ROOT"],
        expanded: new Set(),
      },
      {
        children: { ROOT: nodes },
      }
    );
  })
  .add("30,000 items tree", () => {
    const nodes = Array.from({ length: 1000 }).map((_, i) => `item-${i + 1}`);
    return renderTree(
      {
        getRoots: () => nodes,
        expanded: new Set(
          Array.from({ length: 2000 }).map((_, i) => `item-${i + 1}`)
        ),
      },
      {
        children: Array.from({ length: 2000 }).reduce((res, _, i) => {
          res[`item-${i + 1}`] = [`item-${i + 1001}`];
          return res;
        }, {}),
      }
    );
  });

// Encoding of the following tree/forest:
//
// A
// |-- B
// |   |-- E
// |   |   |-- K
// |   |   `-- L
// |   |-- F
// |   `-- G
// |-- C
// |   |-- H
// |   `-- I
// `-- D
//     `-- J
// M
// `-- N
//     `-- O
// P
// Q
// R
// W
// `-- X
//     `-- Z
// `-- Y
const TEST_TREE = {
  children: {
    A: ["B", "C", "D"],
    B: ["E", "F", "G"],
    C: ["H", "I"],
    D: ["J"],
    E: ["K", "L"],
    F: [],
    G: [],
    H: [],
    I: [],
    J: [],
    K: [],
    L: [],
    M: ["N"],
    N: ["O"],
    O: [],
    P: [],
    Q: [],
    R: [],
    W: ["X", "Y"],
    X: ["Z"],
    Y: [],
    Z: [],
  },
  parent: {
    A: null,
    B: "A",
    C: "A",
    D: "A",
    E: "B",
    F: "B",
    G: "B",
    H: "C",
    I: "C",
    J: "D",
    K: "E",
    L: "E",
    M: null,
    N: "M",
    O: "N",
    P: null,
    Q: null,
    R: null,
    W: null,
    X: "W",
    Y: "W",
    Z: "X",
  },
};

function renderTree(props, tree = TEST_TREE) {
  class container extends Component {
    constructor(props2) {
      super(props2);
      const state = {
        expanded: props2.expanded || new Set(),
        focused: props2.focused,
      };
      delete props2.focused;
      this.state = state;
    }

    render() {
      return createTreeElement(props, this, tree);
    }
  }
  return createFactory(container)();
}

function createTreeElement(props, context, tree) {
  return Tree(
    Object.assign(
      {
        getParent: x => tree.parent && tree.parent[x],
        getChildren: x =>
          tree.children && tree.children[x] ? tree.children[x] : [],
        renderItem: (x, depth, focused, arrow, expanded) => [arrow, x],
        getRoots: () => ["A"],
        getKey: x => `key-${x}`,
        onFocus: x => {
          context.setState(previousState => {
            return { focused: x };
          });
        },
        onExpand: x => {
          context.setState(previousState => {
            const expanded = new Set(previousState.expanded);
            expanded.add(x);
            return { expanded };
          });
        },
        onCollapse: x => {
          context.setState(previousState => {
            const expanded = new Set(previousState.expanded);
            expanded.delete(x);
            return { expanded };
          });
        },
        isExpanded: x => context.state && context.state.expanded.has(x),
        focused: context.state.focused,
      },
      props
    )
  );
}
