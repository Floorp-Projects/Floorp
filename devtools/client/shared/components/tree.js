/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { ViewHelpers } = require("resource://devtools/client/shared/widgets/ViewHelpers.jsm");

const AUTO_EXPAND_DEPTH = 0; // depth

/**
 * An arrow that displays whether its node is expanded (▼) or collapsed
 * (▶). When its node has no children, it is hidden.
 */
const ArrowExpander = createFactory(createClass({
  displayName: "ArrowExpander",

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.item !== nextProps.item
      || this.props.visible !== nextProps.visible
      || this.props.expanded !== nextProps.expanded;
  },

  render() {
    const attrs = {
      className: "arrow theme-twisty",
      onClick: this.props.expanded
        ? () => this.props.onCollapse(this.props.item)
        : e => this.props.onExpand(this.props.item, e.altKey)
    };

    if (this.props.expanded) {
      attrs.className += " open";
    }

    if (!this.props.visible) {
      attrs.style = {
        visibility: "hidden"
      };
    }

    return dom.div(attrs);
  }
}));

const TreeNode = createFactory(createClass({
  componentDidUpdate() {
    if (this.props.focused) {
      this.refs.button.focus();
    }
  },

  render() {
    const arrow = ArrowExpander({
      item: this.props.item,
      expanded: this.props.expanded,
      visible: this.props.hasChildren,
      onExpand: this.props.onExpand,
      onCollapse: this.props.onCollapse
    });

    return dom.div(
      {
        className: "tree-node div",
        onFocus: this.props.onFocus,
        onClick: this.props.onFocus,
        onBlur: this.props.onBlur,
        style: {
          padding: 0,
          margin: 0
        }
      },

      this.props.renderItem(this.props.item,
                            this.props.depth,
                            this.props.focused,
                            arrow,
                            this.props.expanded),

      // XXX: OSX won't focus/blur regular elements even if you set tabindex
      // unless there is an input/button child.
      dom.button(this._buttonAttrs)
    );
  },

  _buttonAttrs: {
    ref: "button",
    style: {
      opacity: 0,
      width: "0 !important",
      height: "0 !important",
      padding: "0 !important",
      outline: "none",
      MozAppearance: "none",
      // XXX: Despite resetting all of the above properties (and margin), the
      // button still ends up with ~79px width, so we set a large negative
      // margin to completely hide it.
      MozMarginStart: "-1000px !important",
    }
  }
}));

/**
 * Create a function that calls the given function `fn` only once per animation
 * frame.
 *
 * @param {Function} fn
 * @returns {Function}
 */
function oncePerAnimationFrame(fn) {
  let animationId = null;
  return function (...args) {
    if (animationId !== null) {
      return;
    }

    animationId = requestAnimationFrame(() => {
      animationId = null;
      fn.call(this, ...args);
    });
  };
}

const NUMBER_OF_OFFSCREEN_ITEMS = 1;

/**
 * A generic tree component. See propTypes for the public API.
 *
 * @see `devtools/client/memory/components/test/mochitest/head.js` for usage
 * @see `devtools/client/memory/components/heap.js` for usage
 */
const Tree = module.exports = createClass({
  displayName: "Tree",

  propTypes: {
    // Required props

    // A function to get an item's parent, or null if it is a root.
    getParent: PropTypes.func.isRequired,
    // A function to get an item's children.
    getChildren: PropTypes.func.isRequired,
    // A function which takes an item and ArrowExpander and returns a
    // component.
    renderItem: PropTypes.func.isRequired,
    // A function which returns the roots of the tree (forest).
    getRoots: PropTypes.func.isRequired,
    // A function to get a unique key for the given item.
    getKey: PropTypes.func.isRequired,
    // The height of an item in the tree including margin and padding, in
    // pixels.
    itemHeight: PropTypes.number.isRequired,

    // Optional props

    // The depth to which we should automatically expand new items.
    autoExpandDepth: PropTypes.number,
    // A predicate that returns true if the last DFS traversal that was cached
    // can be reused, false otherwise. The predicate function is passed the
    // cached traversal as an array of nodes.
    reuseCachedTraversal: PropTypes.func,
    // Optional event handlers for when items are expanded or collapsed.
    onExpand: PropTypes.func,
    onCollapse: PropTypes.func,
  },

  getDefaultProps() {
    return {
      expanded: new Set(),
      seen: new Set(),
      focused: undefined,
      autoExpandDepth: AUTO_EXPAND_DEPTH,
      reuseCachedTraversal: null,
    };
  },

  getInitialState() {
    return {
      scroll: 0,
      height: window.innerHeight,
      expanded: new Set(),
      seen: new Set(),
      focused: undefined,
      cachedTraversal: undefined,
    };
  },

  componentDidMount() {
    window.addEventListener("resize", this._updateHeight);
    this._autoExpand();
    this._updateHeight();
  },

  componentWillUnmount() {
    window.removeEventListener("resize", this._updateHeight);
  },

  componentWillReceiveProps(nextProps) {
    this._autoExpand();
  },

  _autoExpand() {
    if (!this.props.autoExpandDepth) {
      return;
    }

    // Automatically expand the first autoExpandDepth levels for new items. Do
    // not use the usual DFS infrastructure because we don't want to ignore
    // collapsed nodes.
    const autoExpand = (item, currentDepth) => {
      if (currentDepth >= this.props.autoExpandDepth ||
          this.state.seen.has(item)) {
        return;
      }

      this.state.expanded.add(item);
      this.state.seen.add(item);

      for (let child of this.props.getChildren(item)) {
        autoExpand(item, currentDepth + 1);
      }
    };

    for (let root of this.props.getRoots()) {
      autoExpand(root, 0);
    }
  },

  render() {
    const traversal = this._dfsFromRoots();

    // Remove 1 from `begin` and add 2 to `end` so that the top and bottom of
    // the page are filled with the previous and next items respectively,
    // rather than whitespace if the item is not in full view.
    const begin = Math.max(((this.state.scroll / this.props.itemHeight) | 0) - NUMBER_OF_OFFSCREEN_ITEMS, 0);
    const end = begin + (2 * NUMBER_OF_OFFSCREEN_ITEMS) + ((this.state.height / this.props.itemHeight) | 0);
    const toRender = traversal.slice(begin, end);

    const nodes = [
      dom.div({
        key: "top-spacer",
        style: {
          padding: 0,
          margin: 0,
          height: begin * this.props.itemHeight + "px"
        }
      })
    ];

    for (let i = 0; i < toRender.length; i++) {
      let { item, depth } = toRender[i];
      nodes.push(TreeNode({
        key: this.props.getKey(item),
        item: item,
        depth: depth,
        renderItem: this.props.renderItem,
        focused: this.state.focused === item,
        expanded: this.state.expanded.has(item),
        hasChildren: !!this.props.getChildren(item).length,
        onExpand: this._onExpand,
        onCollapse: this._onCollapse,
        onFocus: () => this._onFocus(item)
      }));
    }

    nodes.push(dom.div({
      key: "bottom-spacer",
      style: {
        padding: 0,
        margin: 0,
        height: (traversal.length - 1 - end) * this.props.itemHeight + "px"
      }
    }));

    return dom.div(
      {
        className: "tree",
        ref: "tree",
        onKeyDown: this._onKeyDown,
        onScroll: this._onScroll,
        style: {
          padding: 0,
          margin: 0
        }
      },
      nodes
    );
  },

  /**
   * Updates the state's height based on clientHeight.
   */
  _updateHeight() {
    this.setState({
      height: this.refs.tree.clientHeight
    });
  },

  /**
   * Perform a pre-order depth-first search from item.
   */
  _dfs(item, maxDepth = Infinity, traversal = [], _depth = 0) {
    traversal.push({ item, depth: _depth });

    if (!this.state.expanded.has(item)) {
      return traversal;
    }

    const nextDepth = _depth + 1;

    if (nextDepth > maxDepth) {
      return traversal;
    }

    for (let child of this.props.getChildren(item)) {
      this._dfs(child, maxDepth, traversal, nextDepth);
    }

    return traversal;
  },

  /**
   * Perform a pre-order depth-first search over the whole forest.
   */
  _dfsFromRoots(maxDepth = Infinity) {
    const cached = this.state.cachedTraversal;
    if (cached
        && maxDepth === Infinity
        && this.props.reuseCachedTraversal
        && this.props.reuseCachedTraversal(cached)) {
      return cached;
    }

    const traversal = [];
    for (let root of this.props.getRoots()) {
      this._dfs(root, maxDepth, traversal);
    }

    if (this.props.reuseCachedTraversal) {
      this.state.cachedTraversal = traversal;
    }

    return traversal;
  },

  /**
   * Expands current row.
   *
   * @param {Object} item
   * @param {Boolean} expandAllChildren
   */
  _onExpand: oncePerAnimationFrame(function (item, expandAllChildren) {
    this.state.expanded.add(item);

    if (this.props.onExpand) {
      this.props.onExpand(item);
    }

    if (expandAllChildren) {
      for (let { item: child } of this._dfs(item)) {
        this.state.expanded.add(child);

        if (this.props.onExpand) {
          this.props.onExpand(child);
        }
      }
    }

    this.setState({
      expanded: this.state.expanded,
      cachedTraversal: null,
    });
  }),

  /**
   * Collapses current row.
   *
   * @param {Object} item
   */
  _onCollapse: oncePerAnimationFrame(function (item) {
    this.state.expanded.delete(item);

    if (this.props.onCollapse) {
      this.props.onCollapse(item);
    }

    this.setState({
      expanded: this.state.expanded,
      cachedTraversal: null,
    });
  }),

  /**
   * Sets the passed in item to be the focused item.
   *
   * @param {Object} item
   */
  _onFocus: oncePerAnimationFrame(function (item) {
    this.setState({
      focused: item
    });
  }),

  /**
   * Sets the state to have no focused item.
   */
  _onBlur: oncePerAnimationFrame(function () {
    this.setState({
      focused: undefined
    });
  }),

  /**
   * Fired on a scroll within the tree's container, updates
   * the stored position of the view port to handle virtual view rendering.
   *
   * @param {Event} e
   */
  _onScroll: oncePerAnimationFrame(function (e) {
    this.setState({
      scroll: Math.max(this.refs.tree.scrollTop, 0),
      height: this.refs.tree.clientHeight
    });
  }),

  /**
   * Handles key down events in the tree's container.
   *
   * @param {Event} e
   */
  _onKeyDown(e) {
    if (this.state.focused == null) {
      return;
    }

    // Prevent scrolling when pressing navigation keys. Guard against mocked
    // events received when testing.
    if (e.nativeEvent && e.nativeEvent.preventDefault) {
      ViewHelpers.preventScrolling(e.nativeEvent);
    }

    switch (e.key) {
      case "ArrowUp":
        this._focusPrevNode();
        return false;

      case "ArrowDown":
        this._focusNextNode();
        return false;

      case "ArrowLeft":
        if (this.state.expanded.has(this.state.focused)
            && this.props.getChildren(this.state.focused).length) {
          this._onCollapse(this.state.focused);
        } else {
          this._focusParentNode();
        }
        return false;

      case "ArrowRight":
        if (!this.state.expanded.has(this.state.focused)) {
          this._onExpand(this.state.focused);
        } else {
          this._focusNextNode();
        }
        return false;
      }
  },

  /**
   * Sets the previous node relative to the currently focused item, to focused.
   */
  _focusPrevNode: oncePerAnimationFrame(function () {
    // Start a depth first search and keep going until we reach the currently
    // focused node. Focus the previous node in the DFS, if it exists. If it
    // doesn't exist, we're at the first node already.

    let prev;
    for (let { item } of this._dfsFromRoots()) {
      if (item === this.state.focused) {
        break;
      }
      prev = item;
    }

    if (prev === undefined) {
      return;
    }

    this.setState({
      focused: prev
    });
  }),

  /**
   * Handles the down arrow key which will focus either the next child
   * or sibling row.
   */
  _focusNextNode: oncePerAnimationFrame(function () {
    // Start a depth first search and keep going until we reach the currently
    // focused node. Focus the next node in the DFS, if it exists. If it
    // doesn't exist, we're at the last node already.

    const traversal = this._dfsFromRoots();

    let i = 0;
    for (let { item } of traversal) {
      if (item === this.state.focused) {
        break;
      }
      i++;
    }

    if (i + 1 < traversal.length) {
      this.setState({
        focused: traversal[i + 1].item
      });
    }
  }),

  /**
   * Handles the left arrow key, going back up to the current rows'
   * parent row.
   */
  _focusParentNode: oncePerAnimationFrame(function () {
    const parent = this.props.getParent(this.state.focused);
    if (parent) {
      this.setState({
        focused: parent
      });
    }
  }),
});
