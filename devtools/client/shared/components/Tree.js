/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const React = require("resource://devtools/client/shared/vendor/react.js");
const { Component, createFactory } = React;
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

// Localized strings for (devtools/client/locales/en-US/components.properties)
loader.lazyGetter(this, "L10N_COMPONENTS", function () {
  const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
  return new LocalizationHelper(
    "devtools/client/locales/components.properties"
  );
});

loader.lazyGetter(this, "EXPAND_LABEL", function () {
  return L10N_COMPONENTS.getStr("treeNode.expandButtonTitle");
});

loader.lazyGetter(this, "COLLAPSE_LABEL", function () {
  return L10N_COMPONENTS.getStr("treeNode.collapseButtonTitle");
});

// depth
const AUTO_EXPAND_DEPTH = 0;

// Simplied selector targetting elements that can receive the focus,
// full version at https://stackoverflow.com/questions/1599660.
const FOCUSABLE_SELECTOR = [
  "a[href]:not([tabindex='-1'])",
  "button:not([disabled], [tabindex='-1'])",
  "iframe:not([tabindex='-1'])",
  "input:not([disabled], [tabindex='-1'])",
  "select:not([disabled], [tabindex='-1'])",
  "textarea:not([disabled], [tabindex='-1'])",
  "[tabindex]:not([tabindex='-1'])",
].join(", ");

/**
 * An arrow that displays whether its node is expanded (▼) or collapsed
 * (▶). When its node has no children, it is hidden.
 */
class ArrowExpander extends Component {
  static get propTypes() {
    return {
      expanded: PropTypes.bool,
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.expanded !== nextProps.expanded;
  }

  render() {
    const { expanded } = this.props;

    const classNames = ["arrow"];
    const title = expanded ? COLLAPSE_LABEL : EXPAND_LABEL;

    if (expanded) {
      classNames.push("expanded");
    }
    return dom.button({
      className: classNames.join(" "),
      title,
    });
  }
}

const treeIndent = dom.span({ className: "tree-indent" }, "\u200B");
const treeLastIndent = dom.span(
  { className: "tree-indent tree-last-indent" },
  "\u200B"
);

class TreeNode extends Component {
  static get propTypes() {
    return {
      id: PropTypes.any.isRequired,
      index: PropTypes.number.isRequired,
      depth: PropTypes.number.isRequired,
      focused: PropTypes.bool.isRequired,
      active: PropTypes.bool.isRequired,
      expanded: PropTypes.bool.isRequired,
      item: PropTypes.any.isRequired,
      isExpandable: PropTypes.bool.isRequired,
      onClick: PropTypes.func,
      shouldItemUpdate: PropTypes.func,
      renderItem: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.treeNodeRef = React.createRef();

    this._onKeyDown = this._onKeyDown.bind(this);
  }

  componentDidMount() {
    // Make sure that none of the focusable elements inside the tree node
    // container are tabbable if the tree node is not active. If the tree node
    // is active and focus is outside its container, focus on the first
    // focusable element inside.
    const elms = this.getFocusableElements();
    if (this.props.active) {
      const doc = this.treeNodeRef.current.ownerDocument;
      if (elms.length && !elms.includes(doc.activeElement)) {
        elms[0].focus();
      }
    } else {
      elms.forEach(elm => elm.setAttribute("tabindex", "-1"));
    }
  }

  shouldComponentUpdate(nextProps) {
    return (
      this.props.item !== nextProps.item ||
      (this.props.shouldItemUpdate &&
        this.props.shouldItemUpdate(this.props.item, nextProps.item)) ||
      this.props.focused !== nextProps.focused ||
      this.props.expanded !== nextProps.expanded
    );
  }

  /**
   * Get a list of all elements that are focusable with a keyboard inside the
   * tree node.
   */
  getFocusableElements() {
    return this.treeNodeRef.current
      ? Array.from(
          this.treeNodeRef.current.querySelectorAll(FOCUSABLE_SELECTOR)
        )
      : [];
  }

  /**
   * Wrap and move keyboard focus to first/last focusable element inside the
   * tree node to prevent the focus from escaping the tree node boundaries.
   * element).
   *
   * @param  {DOMNode} current  currently focused element
   * @param  {Boolean} back     direction
   * @return {Boolean}          true there is a newly focused element.
   */
  _wrapMoveFocus(current, back) {
    const elms = this.getFocusableElements();
    let next;

    if (elms.length === 0) {
      return false;
    }

    if (back) {
      if (elms.indexOf(current) === 0) {
        next = elms[elms.length - 1];
        next.focus();
      }
    } else if (elms.indexOf(current) === elms.length - 1) {
      next = elms[0];
      next.focus();
    }

    return !!next;
  }

  _onKeyDown(e) {
    const { target, key, shiftKey } = e;

    if (key !== "Tab") {
      return;
    }

    const focusMoved = this._wrapMoveFocus(target, shiftKey);
    if (focusMoved) {
      // Focus was moved to the begining/end of the list, so we need to prevent
      // the default focus change that would happen here.
      e.preventDefault();
    }

    e.stopPropagation();
  }

  render() {
    const {
      depth,
      id,
      item,
      focused,
      active,
      expanded,
      renderItem,
      isExpandable,
    } = this.props;

    const arrow = isExpandable
      ? ArrowExpanderFactory({
          item,
          expanded,
        })
      : null;

    let ariaExpanded;
    if (this.props.isExpandable) {
      ariaExpanded = false;
    }
    if (this.props.expanded) {
      ariaExpanded = true;
    }

    const indents = Array.from({ length: depth }, (_, i) => {
      if (i == depth - 1) {
        return treeLastIndent;
      }
      return treeIndent;
    });

    const items = indents.concat(
      renderItem(item, depth, focused, arrow, expanded)
    );

    return dom.div(
      {
        id,
        className: `tree-node${focused ? " focused" : ""}${
          active ? " active" : ""
        }`,
        onClick: this.props.onClick,
        onKeyDownCapture: active ? this._onKeyDown : null,
        role: "treeitem",
        ref: this.treeNodeRef,
        "aria-level": depth + 1,
        "aria-expanded": ariaExpanded,
        "data-expandable": this.props.isExpandable,
      },
      ...items
    );
  }
}

const ArrowExpanderFactory = createFactory(ArrowExpander);
const TreeNodeFactory = createFactory(TreeNode);

/**
 * Create a function that calls the given function `fn` only once per animation
 * frame.
 *
 * @param {Function} fn
 * @param {Object} options: object that contains the following properties:
 *                      - {Function} getDocument: A function that return the document
 *                                                the component is rendered in.
 * @returns {Function}
 */
function oncePerAnimationFrame(fn, { getDocument }) {
  let animationId = null;
  let argsToPass = null;
  return function (...args) {
    argsToPass = args;
    if (animationId !== null) {
      return;
    }

    const doc = getDocument();
    if (!doc) {
      return;
    }

    animationId = doc.defaultView.requestAnimationFrame(() => {
      fn.call(this, ...argsToPass);
      animationId = null;
      argsToPass = null;
    });
  };
}

/**
 * A generic tree component. See propTypes for the public API.
 *
 * This tree component doesn't make any assumptions about the structure of your
 * tree data. Whether children are computed on demand, or stored in an array in
 * the parent's `_children` property, it doesn't matter. We only require the
 * implementation of `getChildren`, `getRoots`, `getParent`, and `isExpanded`
 * functions.
 *
 * This tree component is well tested and reliable. See the tests in ./tests
 * and its usage in the memory panel in mozilla-central.
 *
 * This tree component doesn't make any assumptions about how to render items in
 * the tree. You provide a `renderItem` function, and this component will ensure
 * that only those items whose parents are expanded and which are visible in the
 * viewport are rendered. The `renderItem` function could render the items as a
 * "traditional" tree or as rows in a table or anything else. It doesn't
 * restrict you to only one certain kind of tree.
 *
 * The tree comes with basic styling for the indent, the arrow, as well as
 * hovered and focused styles which can be override in CSS.
 *
 * ### Example Usage
 *
 * Suppose we have some tree data where each item has this form:
 *
 *     {
 *       id: Number,
 *       label: String,
 *       parent: Item or null,
 *       children: Array of child items,
 *       expanded: bool,
 *     }
 *
 * Here is how we could render that data with this component:
 *
 *     class MyTree extends Component {
 *       static get propTypes() {
 *         // The root item of the tree, with the form described above.
 *         return {
 *           root: PropTypes.object.isRequired
 *         };
 *       },
 *
 *       render() {
 *         return Tree({
 *           getRoots: () => [this.props.root],
 *
 *           getParent: item => item.parent,
 *           getChildren: item => item.children,
 *           getKey: item => item.id,
 *           isExpanded: item => item.expanded,
 *
 *           renderItem: (item, depth, isFocused, arrow, isExpanded) => {
 *             let className = "my-tree-item";
 *             if (isFocused) {
 *               className += " focused";
 *             }
 *             return dom.div({
 *               className,
 *             },
 *               arrow,
 *               // And here is the label for this item.
 *               dom.span({ className: "my-tree-item-label" }, item.label)
 *             );
 *           },
 *
 *           onExpand: item => dispatchExpandActionToRedux(item),
 *           onCollapse: item => dispatchCollapseActionToRedux(item),
 *         });
 *       }
 *     }
 */
class Tree extends Component {
  static get propTypes() {
    return {
      // Required props

      // A function to get an item's parent, or null if it is a root.
      //
      // Type: getParent(item: Item) -> Maybe<Item>
      //
      // Example:
      //
      //     // The parent of this item is stored in its `parent` property.
      //     getParent: item => item.parent
      getParent: PropTypes.func.isRequired,

      // A function to get an item's children.
      //
      // Type: getChildren(item: Item) -> [Item]
      //
      // Example:
      //
      //     // This item's children are stored in its `children` property.
      //     getChildren: item => item.children
      getChildren: PropTypes.func.isRequired,

      // A function to check if the tree node for the item should be updated.
      //
      // Type: shouldItemUpdate(prevItem: Item, nextItem: Item) -> Boolean
      //
      // Example:
      //
      //     // This item should be updated if it's type is a long string
      //     shouldItemUpdate: (prevItem, nextItem) =>
      //       nextItem.type === "longstring"
      shouldItemUpdate: PropTypes.func,

      // A function which takes an item and ArrowExpander component instance and
      // returns a component, or text, or anything else that React considers
      // renderable.
      //
      // Type: renderItem(item: Item,
      //                  depth: Number,
      //                  isFocused: Boolean,
      //                  arrow: ReactComponent,
      //                  isExpanded: Boolean) -> ReactRenderable
      //
      // Example:
      //
      //     renderItem: (item, depth, isFocused, arrow, isExpanded) => {
      //       let className = "my-tree-item";
      //       if (isFocused) {
      //         className += " focused";
      //       }
      //       return dom.div(
      //         {
      //           className,
      //           style: { marginLeft: depth * 10 + "px" }
      //         },
      //         arrow,
      //         dom.span({ className: "my-tree-item-label" }, item.label)
      //       );
      //     },
      renderItem: PropTypes.func.isRequired,

      // A function which returns the roots of the tree (forest).
      //
      // Type: getRoots() -> [Item]
      //
      // Example:
      //
      //     // In this case, we only have one top level, root item. You could
      //     // return multiple items if you have many top level items in your
      //     // tree.
      //     getRoots: () => [this.props.rootOfMyTree]
      getRoots: PropTypes.func.isRequired,

      // A function to get a unique key for the given item. This helps speed up
      // React's rendering a *TON*.
      //
      // Type: getKey(item: Item) -> String
      //
      // Example:
      //
      //     getKey: item => `my-tree-item-${item.uniqueId}`
      getKey: PropTypes.func.isRequired,

      // A function to get whether an item is expanded or not. If an item is not
      // expanded, then it must be collapsed.
      //
      // Type: isExpanded(item: Item) -> Boolean
      //
      // Example:
      //
      //     isExpanded: item => item.expanded,
      isExpanded: PropTypes.func.isRequired,

      // Optional props

      // The currently focused item, if any such item exists.
      focused: PropTypes.any,

      // Handle when a new item is focused.
      onFocus: PropTypes.func,

      // The depth to which we should automatically expand new items.
      autoExpandDepth: PropTypes.number,
      // Should auto expand all new items or just the new items under the first
      // root item.
      autoExpandAll: PropTypes.bool,

      // Auto expand a node only if number of its children
      // are less than autoExpandNodeChildrenLimit
      autoExpandNodeChildrenLimit: PropTypes.number,

      // Note: the two properties below are mutually exclusive. Only one of the
      // label properties is necessary.
      // ID of an element whose textual content serves as an accessible label
      // for a tree.
      labelledby: PropTypes.string,
      // Accessibility label for a tree widget.
      label: PropTypes.string,

      // Optional event handlers for when items are expanded or collapsed.
      // Useful for dispatching redux events and updating application state,
      // maybe lazily loading subtrees from a worker, etc.
      //
      // Type:
      //     onExpand(item: Item)
      //     onCollapse(item: Item)
      //
      // Example:
      //
      //     onExpand: item => dispatchExpandActionToRedux(item)
      onExpand: PropTypes.func,
      onCollapse: PropTypes.func,
      // The currently active (keyboard) item, if any such item exists.
      active: PropTypes.any,
      // Optional event handler called with the current focused node when the
      // Enter key is pressed. Can be useful to allow further keyboard actions
      // within the tree node.
      onActivate: PropTypes.func,
      isExpandable: PropTypes.func,
      // Additional classes to add to the root element.
      className: PropTypes.string,
      // style object to be applied to the root element.
      style: PropTypes.object,
      // Prevents blur when Tree loses focus
      preventBlur: PropTypes.bool,
      initiallyExpanded: PropTypes.func,
    };
  }

  static get defaultProps() {
    return {
      autoExpandDepth: AUTO_EXPAND_DEPTH,
      autoExpandAll: true,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      autoExpanded: new Set(),
    };

    this.treeRef = React.createRef();

    const opaf = fn =>
      oncePerAnimationFrame(fn, {
        getDocument: () =>
          this.treeRef.current && this.treeRef.current.ownerDocument,
      });

    this._onExpand = opaf(this._onExpand).bind(this);
    this._onCollapse = opaf(this._onCollapse).bind(this);
    this._focusPrevNode = opaf(this._focusPrevNode).bind(this);
    this._focusNextNode = opaf(this._focusNextNode).bind(this);
    this._focusParentNode = opaf(this._focusParentNode).bind(this);
    this._focusFirstNode = opaf(this._focusFirstNode).bind(this);
    this._focusLastNode = opaf(this._focusLastNode).bind(this);

    this._autoExpand = this._autoExpand.bind(this);
    this._preventArrowKeyScrolling = this._preventArrowKeyScrolling.bind(this);
    this._preventEvent = this._preventEvent.bind(this);
    this._dfs = this._dfs.bind(this);
    this._dfsFromRoots = this._dfsFromRoots.bind(this);
    this._focus = this._focus.bind(this);
    this._activate = this._activate.bind(this);
    this._scrollNodeIntoView = this._scrollNodeIntoView.bind(this);
    this._onBlur = this._onBlur.bind(this);
    this._onKeyDown = this._onKeyDown.bind(this);
    this._nodeIsExpandable = this._nodeIsExpandable.bind(this);
  }

  componentDidMount() {
    this._autoExpand();
    if (this.props.focused) {
      this._scrollNodeIntoView(this.props.focused);
    }
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    this._autoExpand();
  }

  componentDidUpdate(prevProps, prevState) {
    if (this.props.focused && prevProps.focused !== this.props.focused) {
      this._scrollNodeIntoView(this.props.focused);
    }
  }

  _autoExpand() {
    const { autoExpandDepth, autoExpandNodeChildrenLimit, initiallyExpanded } =
      this.props;

    if (!autoExpandDepth && !initiallyExpanded) {
      return;
    }

    // Automatically expand the first autoExpandDepth levels for new items. Do
    // not use the usual DFS infrastructure because we don't want to ignore
    // collapsed nodes. Any initially expanded items will be expanded regardless
    // of how deep they are.
    const autoExpand = (item, currentDepth) => {
      const initial = initiallyExpanded && initiallyExpanded(item);

      if (!initial && currentDepth >= autoExpandDepth) {
        return;
      }

      const children = this.props.getChildren(item);
      if (
        !initial &&
        autoExpandNodeChildrenLimit &&
        children.length > autoExpandNodeChildrenLimit
      ) {
        return;
      }

      if (!this.state.autoExpanded.has(item)) {
        this.props.onExpand(item);
        this.state.autoExpanded.add(item);
      }

      const length = children.length;
      for (let i = 0; i < length; i++) {
        autoExpand(children[i], currentDepth + 1);
      }
    };

    const roots = this.props.getRoots();
    const length = roots.length;
    if (this.props.autoExpandAll) {
      for (let i = 0; i < length; i++) {
        autoExpand(roots[i], 0);
      }
    } else if (length != 0) {
      autoExpand(roots[0], 0);

      if (initiallyExpanded) {
        for (let i = 1; i < length; i++) {
          if (initiallyExpanded(roots[i])) {
            autoExpand(roots[i], 0);
          }
        }
      }
    }
  }

  _preventArrowKeyScrolling(e) {
    switch (e.key) {
      case "ArrowUp":
      case "ArrowDown":
      case "ArrowLeft":
      case "ArrowRight":
        this._preventEvent(e);
        break;
    }
  }

  _preventEvent(e) {
    e.preventDefault();
    e.stopPropagation();
    if (e.nativeEvent) {
      if (e.nativeEvent.preventDefault) {
        e.nativeEvent.preventDefault();
      }
      if (e.nativeEvent.stopPropagation) {
        e.nativeEvent.stopPropagation();
      }
    }
  }

  /**
   * Perform a pre-order depth-first search from item.
   */
  _dfs(item, maxDepth = Infinity, traversal = [], _depth = 0) {
    traversal.push({ item, depth: _depth });

    if (!this.props.isExpanded(item)) {
      return traversal;
    }

    const nextDepth = _depth + 1;

    if (nextDepth > maxDepth) {
      return traversal;
    }

    const children = this.props.getChildren(item);
    const length = children.length;
    for (let i = 0; i < length; i++) {
      this._dfs(children[i], maxDepth, traversal, nextDepth);
    }

    return traversal;
  }

  /**
   * Perform a pre-order depth-first search over the whole forest.
   */
  _dfsFromRoots(maxDepth = Infinity) {
    const traversal = [];

    const roots = this.props.getRoots();
    const length = roots.length;
    for (let i = 0; i < length; i++) {
      this._dfs(roots[i], maxDepth, traversal);
    }

    return traversal;
  }

  /**
   * Expands current row.
   *
   * @param {Object} item
   * @param {Boolean} expandAllChildren
   */
  _onExpand(item, expandAllChildren) {
    if (this.props.onExpand) {
      this.props.onExpand(item);

      if (expandAllChildren) {
        const children = this._dfs(item);
        const length = children.length;
        for (let i = 0; i < length; i++) {
          this.props.onExpand(children[i].item);
        }
      }
    }
  }

  /**
   * Collapses current row.
   *
   * @param {Object} item
   */
  _onCollapse(item) {
    if (this.props.onCollapse) {
      this.props.onCollapse(item);
    }
  }

  /**
   * Sets the passed in item to be the focused item.
   *
   * @param {Object|undefined} item
   *        The item to be focused, or undefined to focus no item.
   *
   * @param {Object|undefined} options
   *        An options object which can contain:
   *          - dir: "up" or "down" to indicate if we should scroll the element
   *                 to the top or the bottom of the scrollable container when
   *                 the element is off canvas.
   */
  _focus(item, options = {}) {
    const { preventAutoScroll } = options;
    if (item && !preventAutoScroll) {
      this._scrollNodeIntoView(item, options);
    }

    if (this.props.active != undefined) {
      this._activate(undefined);
      const doc = this.treeRef.current && this.treeRef.current.ownerDocument;
      if (this.treeRef.current !== doc.activeElement) {
        this.treeRef.current.focus();
      }
    }

    if (this.props.onFocus) {
      this.props.onFocus(item);
    }
  }

  /**
   * Sets the passed in item to be the active item.
   *
   * @param {Object|undefined} item
   *        The item to be activated, or undefined to activate no item.
   */
  _activate(item) {
    if (this.props.onActivate) {
      this.props.onActivate(item);
    }
  }

  /**
   * Sets the passed in item to be the focused item.
   *
   * @param {Object|undefined} item
   *        The item to be scrolled to.
   *
   * @param {Object|undefined} options
   *        An options object which can contain:
   *          - dir: "up" or "down" to indicate if we should scroll the element
   *                 to the top or the bottom of the scrollable container when
   *                 the element is off canvas.
   */
  _scrollNodeIntoView(item, options = {}) {
    if (item !== undefined) {
      const treeElement = this.treeRef.current;
      const doc = treeElement && treeElement.ownerDocument;
      const element = doc.getElementById(this.props.getKey(item));

      if (element) {
        const { top, bottom } = element.getBoundingClientRect();
        const closestScrolledParent = node => {
          if (node == null) {
            return null;
          }

          if (node.scrollHeight > node.clientHeight) {
            return node;
          }
          return closestScrolledParent(node.parentNode);
        };
        const scrolledParent = closestScrolledParent(treeElement);
        const scrolledParentRect = scrolledParent
          ? scrolledParent.getBoundingClientRect()
          : null;
        const isVisible =
          !scrolledParent ||
          (top >= scrolledParentRect.top &&
            bottom <= scrolledParentRect.bottom);

        if (!isVisible) {
          const { alignTo } = options;
          const scrollToTop = alignTo
            ? alignTo === "top"
            : !scrolledParentRect || top < scrolledParentRect.top;
          element.scrollIntoView(scrollToTop);
        }
      }
    }
  }

  /**
   * Sets the state to have no focused item.
   */
  _onBlur(e) {
    if (this.props.active != undefined) {
      const { relatedTarget } = e;
      if (!this.treeRef.current.contains(relatedTarget)) {
        this._activate(undefined);
      }
    } else if (!this.props.preventBlur) {
      this._focus(undefined);
    }
  }

  /**
   * Handles key down events in the tree's container.
   *
   * @param {Event} e
   */
  // eslint-disable-next-line complexity
  _onKeyDown(e) {
    if (this.props.focused == null) {
      return;
    }

    // Allow parent nodes to use navigation arrows with modifiers.
    if (e.altKey || e.ctrlKey || e.shiftKey || e.metaKey) {
      return;
    }

    this._preventArrowKeyScrolling(e);
    const doc = this.treeRef.current && this.treeRef.current.ownerDocument;

    switch (e.key) {
      case "ArrowUp":
        this._focusPrevNode();
        return;

      case "ArrowDown":
        this._focusNextNode();
        return;

      case "ArrowLeft":
        if (
          this.props.isExpanded(this.props.focused) &&
          this._nodeIsExpandable(this.props.focused)
        ) {
          this._onCollapse(this.props.focused);
        } else {
          this._focusParentNode();
        }
        return;

      case "ArrowRight":
        if (
          this._nodeIsExpandable(this.props.focused) &&
          !this.props.isExpanded(this.props.focused)
        ) {
          this._onExpand(this.props.focused);
        } else {
          this._focusNextNode();
        }
        return;

      case "Home":
        this._focusFirstNode();
        return;

      case "End":
        this._focusLastNode();
        return;

      case "Enter":
      case " ":
        if (this.treeRef.current === doc.activeElement) {
          this._preventEvent(e);
          if (this.props.active !== this.props.focused) {
            this._activate(this.props.focused);
          }
        }
        return;

      case "Escape":
        this._preventEvent(e);
        if (this.props.active != undefined) {
          this._activate(undefined);
        }

        if (this.treeRef.current !== doc.activeElement) {
          this.treeRef.current.focus();
        }
    }
  }

  /**
   * Sets the previous node relative to the currently focused item, to focused.
   */
  _focusPrevNode() {
    // Start a depth first search and keep going until we reach the currently
    // focused node. Focus the previous node in the DFS, if it exists. If it
    // doesn't exist, we're at the first node already.

    let prev;

    const traversal = this._dfsFromRoots();
    const length = traversal.length;
    for (let i = 0; i < length; i++) {
      const item = traversal[i].item;
      if (item === this.props.focused) {
        break;
      }
      prev = item;
    }
    if (prev === undefined) {
      return;
    }

    this._focus(prev, { alignTo: "top" });
  }

  /**
   * Handles the down arrow key which will focus either the next child
   * or sibling row.
   */
  _focusNextNode() {
    // Start a depth first search and keep going until we reach the currently
    // focused node. Focus the next node in the DFS, if it exists. If it
    // doesn't exist, we're at the last node already.
    const traversal = this._dfsFromRoots();
    const length = traversal.length;
    let i = 0;

    while (i < length) {
      if (traversal[i].item === this.props.focused) {
        break;
      }
      i++;
    }

    if (i + 1 < traversal.length) {
      this._focus(traversal[i + 1].item, { alignTo: "bottom" });
    }
  }

  /**
   * Handles the left arrow key, going back up to the current rows'
   * parent row.
   */
  _focusParentNode() {
    const parent = this.props.getParent(this.props.focused);
    if (!parent) {
      this._focusPrevNode(this.props.focused);
      return;
    }

    this._focus(parent, { alignTo: "top" });
  }

  _focusFirstNode() {
    const traversal = this._dfsFromRoots();
    this._focus(traversal[0].item, { alignTo: "top" });
  }

  _focusLastNode() {
    const traversal = this._dfsFromRoots();
    const lastIndex = traversal.length - 1;
    this._focus(traversal[lastIndex].item, { alignTo: "bottom" });
  }

  _nodeIsExpandable(item) {
    return this.props.isExpandable
      ? this.props.isExpandable(item)
      : !!this.props.getChildren(item).length;
  }

  render() {
    const traversal = this._dfsFromRoots();
    const { active, focused } = this.props;

    const nodes = traversal.map((v, i) => {
      const { item, depth } = traversal[i];
      const key = this.props.getKey(item, i);
      const focusedKey = focused ? this.props.getKey(focused, i) : null;
      return TreeNodeFactory({
        // We make a key unique depending on whether the tree node is in active
        // or inactive state to make sure that it is actually replaced and the
        // tabbable state is reset.
        key: `${key}-${active === item ? "active" : "inactive"}`,
        id: key,
        index: i,
        item,
        depth,
        shouldItemUpdate: this.props.shouldItemUpdate,
        renderItem: this.props.renderItem,
        focused: focusedKey === key,
        active: active === item,
        expanded: this.props.isExpanded(item),
        isExpandable: this._nodeIsExpandable(item),
        onExpand: this._onExpand,
        onCollapse: this._onCollapse,
        onClick: e => {
          // We can stop the propagation since click handler on the node can be
          // created in `renderItem`.
          e.stopPropagation();

          // Since the user just clicked the node, there's no need to check if
          // it should be scrolled into view.
          this._focus(item, { preventAutoScroll: true });
          if (this.props.isExpanded(item)) {
            this.props.onCollapse(item, e.altKey);
          } else {
            this.props.onExpand(item, e.altKey);
          }

          // Focus should always remain on the tree container itself.
          this.treeRef.current.focus();
        },
      });
    });

    const style = Object.assign({}, this.props.style || {});

    return dom.div(
      {
        className: `tree ${this.props.className ? this.props.className : ""}`,
        ref: this.treeRef,
        role: "tree",
        tabIndex: "0",
        onKeyDown: this._onKeyDown,
        onKeyPress: this._preventArrowKeyScrolling,
        onKeyUp: this._preventArrowKeyScrolling,
        onFocus: ({ nativeEvent }) => {
          if (focused || !nativeEvent || !this.treeRef.current) {
            return;
          }

          const { explicitOriginalTarget } = nativeEvent;
          // Only set default focus to the first tree node if the focus came
          // from outside the tree (e.g. by tabbing to the tree from other
          // external elements).
          if (
            explicitOriginalTarget !== this.treeRef.current &&
            !this.treeRef.current.contains(explicitOriginalTarget)
          ) {
            this._focus(traversal[0].item);
          }
        },
        onBlur: this._onBlur,
        "aria-label": this.props.label,
        "aria-labelledby": this.props.labelledby,
        "aria-activedescendant": focused && this.props.getKey(focused),
        style,
      },
      nodes
    );
  }
}

module.exports = Tree;
