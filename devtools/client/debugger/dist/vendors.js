/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory(require("devtools/client/shared/vendor/react-prop-types"), require("devtools/client/shared/vendor/react-dom-factories"), require("devtools/client/shared/vendor/react"), require("Services"), require("devtools/shared/flags"), require("devtools/client/shared/vendor/react-dom"), require("devtools/client/shared/vendor/lodash"), require("devtools/client/framework/menu"), require("devtools/client/framework/menu-item"));
	else if(typeof define === 'function' && define.amd)
		define(["devtools/client/shared/vendor/react-prop-types", "devtools/client/shared/vendor/react-dom-factories", "devtools/client/shared/vendor/react", "Services", "devtools/shared/flags", "devtools/client/shared/vendor/react-dom", "devtools/client/shared/vendor/lodash", "devtools/client/framework/menu", "devtools/client/framework/menu-item"], factory);
	else {
		var a = typeof exports === 'object' ? factory(require("devtools/client/shared/vendor/react-prop-types"), require("devtools/client/shared/vendor/react-dom-factories"), require("devtools/client/shared/vendor/react"), require("Services"), require("devtools/shared/flags"), require("devtools/client/shared/vendor/react-dom"), require("devtools/client/shared/vendor/lodash"), require("devtools/client/framework/menu"), require("devtools/client/framework/menu-item")) : factory(root["devtools/client/shared/vendor/react-prop-types"], root["devtools/client/shared/vendor/react-dom-factories"], root["devtools/client/shared/vendor/react"], root["Services"], root["devtools/shared/flags"], root["devtools/client/shared/vendor/react-dom"], root["devtools/client/shared/vendor/lodash"], root["devtools/client/framework/menu"], root["devtools/client/framework/menu-item"]);
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(typeof self !== 'undefined' ? self : this, function(__WEBPACK_EXTERNAL_MODULE_0__, __WEBPACK_EXTERNAL_MODULE_1__, __WEBPACK_EXTERNAL_MODULE_6__, __WEBPACK_EXTERNAL_MODULE_37__, __WEBPACK_EXTERNAL_MODULE_103__, __WEBPACK_EXTERNAL_MODULE_112__, __WEBPACK_EXTERNAL_MODULE_417__, __WEBPACK_EXTERNAL_MODULE_490__, __WEBPACK_EXTERNAL_MODULE_491__) {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, {
/******/ 				configurable: false,
/******/ 				enumerable: true,
/******/ 				get: getter
/******/ 			});
/******/ 		}
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "/assets/build";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 413);
/******/ })
/************************************************************************/
/******/ ({

/***/ 0:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_0__;

/***/ }),

/***/ 1:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_1__;

/***/ }),

/***/ 102:
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(process) {/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const flag = __webpack_require__(103);

function isBrowser() {
  return typeof window == "object";
}

function isNode() {
  return process && process.release && process.release.name == 'node';
}

function isDevelopment() {
  if (!isNode() && isBrowser()) {
    const href = window.location ? window.location.href : "";
    return href.match(/^file:/) || href.match(/localhost:/);
  }

  return "production" != "production";
}

function isTesting() {
  return flag.testing;
}

function isFirefoxPanel() {
  return !isDevelopment();
}

function isFirefox() {
  return /firefox/i.test(navigator.userAgent);
}

module.exports = {
  isDevelopment,
  isTesting,
  isFirefoxPanel,
  isFirefox
};
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(35)))

/***/ }),

/***/ 103:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_103__;

/***/ }),

/***/ 108:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _tree = _interopRequireDefault(__webpack_require__(109));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
module.exports = {
  Tree: _tree.default
};

/***/ }),

/***/ 109:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _react = _interopRequireDefault(__webpack_require__(6));

var _reactDomFactories = _interopRequireDefault(__webpack_require__(1));

var _propTypes = _interopRequireDefault(__webpack_require__(0));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  Component,
  createFactory
} = _react.default;

__webpack_require__(110); // depth


const AUTO_EXPAND_DEPTH = 0; // Simplied selector targetting elements that can receive the focus,
// full version at https://stackoverflow.com/questions/1599660.

const FOCUSABLE_SELECTOR = ["a[href]:not([tabindex='-1'])", "button:not([disabled]):not([tabindex='-1'])", "iframe:not([tabindex='-1'])", "input:not([disabled]):not([tabindex='-1'])", "select:not([disabled]):not([tabindex='-1'])", "textarea:not([disabled]):not([tabindex='-1'])", "[tabindex]:not([tabindex='-1'])"].join(", ");
/**
 * An arrow that displays whether its node is expanded (▼) or collapsed
 * (▶). When its node has no children, it is hidden.
 */

class ArrowExpander extends Component {
  static get propTypes() {
    return {
      expanded: _propTypes.default.bool
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.expanded !== nextProps.expanded;
  }

  render() {
    const {
      expanded
    } = this.props;
    const classNames = ["arrow"];

    if (expanded) {
      classNames.push("expanded");
    }

    return _reactDomFactories.default.button({
      className: classNames.join(" ")
    });
  }

}

const treeIndent = _reactDomFactories.default.span({
  className: "tree-indent"
}, "\u200B");

const treeLastIndent = _reactDomFactories.default.span({
  className: "tree-indent tree-last-indent"
}, "\u200B");

class TreeNode extends Component {
  static get propTypes() {
    return {
      id: _propTypes.default.any.isRequired,
      index: _propTypes.default.number.isRequired,
      depth: _propTypes.default.number.isRequired,
      focused: _propTypes.default.bool.isRequired,
      active: _propTypes.default.bool.isRequired,
      expanded: _propTypes.default.bool.isRequired,
      item: _propTypes.default.any.isRequired,
      isExpandable: _propTypes.default.bool.isRequired,
      onClick: _propTypes.default.func,
      shouldItemUpdate: _propTypes.default.func,
      renderItem: _propTypes.default.func.isRequired
    };
  }

  constructor(props) {
    super(props);
    this.treeNodeRef = _react.default.createRef();
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

      if (elms.length > 0 && !elms.includes(doc.activeElement)) {
        elms[0].focus();
      }
    } else {
      elms.forEach(elm => elm.setAttribute("tabindex", "-1"));
    }
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item !== nextProps.item || this.props.shouldItemUpdate && this.props.shouldItemUpdate(this.props.item, nextProps.item) || this.props.focused !== nextProps.focused || this.props.expanded !== nextProps.expanded;
  }
  /**
   * Get a list of all elements that are focusable with a keyboard inside the
   * tree node.
   */


  getFocusableElements() {
    return this.treeNodeRef.current ? Array.from(this.treeNodeRef.current.querySelectorAll(FOCUSABLE_SELECTOR)) : [];
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
    const {
      target,
      key,
      shiftKey
    } = e;

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
      isExpandable
    } = this.props;
    const arrow = isExpandable ? ArrowExpanderFactory({
      item,
      expanded
    }) : null;
    let ariaExpanded;

    if (this.props.isExpandable) {
      ariaExpanded = false;
    }

    if (this.props.expanded) {
      ariaExpanded = true;
    }

    const indents = Array.from({
      length: depth
    }, (_, i) => {
      if (i == depth - 1) {
        return treeLastIndent;
      }

      return treeIndent;
    });
    const items = indents.concat(renderItem(item, depth, focused, arrow, expanded));
    return _reactDomFactories.default.div({
      id,
      className: `tree-node${focused ? " focused" : ""}${active ? " active" : ""}`,
      onClick: this.props.onClick,
      onKeyDownCapture: active ? this._onKeyDown : null,
      role: "treeitem",
      ref: this.treeNodeRef,
      "aria-level": depth + 1,
      "aria-expanded": ariaExpanded,
      "data-expandable": this.props.isExpandable
    }, ...items);
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

function oncePerAnimationFrame(fn, {
  getDocument
}) {
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
 * and its usage in the performance and memory panels in mozilla-central.
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
 *           itemHeight: 20, // px
 *
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
      getParent: _propTypes.default.func.isRequired,
      // A function to get an item's children.
      //
      // Type: getChildren(item: Item) -> [Item]
      //
      // Example:
      //
      //     // This item's children are stored in its `children` property.
      //     getChildren: item => item.children
      getChildren: _propTypes.default.func.isRequired,
      // A function to check if the tree node for the item should be updated.
      //
      // Type: shouldItemUpdate(prevItem: Item, nextItem: Item) -> Boolean
      //
      // Example:
      //
      //     // This item should be updated if it's type is a long string
      //     shouldItemUpdate: (prevItem, nextItem) =>
      //       nextItem.type === "longstring"
      shouldItemUpdate: _propTypes.default.func,
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
      renderItem: _propTypes.default.func.isRequired,
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
      getRoots: _propTypes.default.func.isRequired,
      // A function to get a unique key for the given item. This helps speed up
      // React's rendering a *TON*.
      //
      // Type: getKey(item: Item) -> String
      //
      // Example:
      //
      //     getKey: item => `my-tree-item-${item.uniqueId}`
      getKey: _propTypes.default.func.isRequired,
      // A function to get whether an item is expanded or not. If an item is not
      // expanded, then it must be collapsed.
      //
      // Type: isExpanded(item: Item) -> Boolean
      //
      // Example:
      //
      //     isExpanded: item => item.expanded,
      isExpanded: _propTypes.default.func.isRequired,
      // Optional props
      // The currently focused item, if any such item exists.
      focused: _propTypes.default.any,
      // Handle when a new item is focused.
      onFocus: _propTypes.default.func,
      // The depth to which we should automatically expand new items.
      autoExpandDepth: _propTypes.default.number,
      // Should auto expand all new items or just the new items under the first
      // root item.
      autoExpandAll: _propTypes.default.bool,
      // Auto expand a node only if number of its children
      // are less than autoExpandNodeChildrenLimit
      autoExpandNodeChildrenLimit: _propTypes.default.number,
      // Note: the two properties below are mutually exclusive. Only one of the
      // label properties is necessary.
      // ID of an element whose textual content serves as an accessible label
      // for a tree.
      labelledby: _propTypes.default.string,
      // Accessibility label for a tree widget.
      label: _propTypes.default.string,
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
      onExpand: _propTypes.default.func,
      onCollapse: _propTypes.default.func,
      // The currently active (keyboard) item, if any such item exists.
      active: _propTypes.default.any,
      // Optional event handler called with the current focused node when the
      // Enter key is pressed. Can be useful to allow further keyboard actions
      // within the tree node.
      onActivate: _propTypes.default.func,
      isExpandable: _propTypes.default.func,
      // Additional classes to add to the root element.
      className: _propTypes.default.string,
      // style object to be applied to the root element.
      style: _propTypes.default.object,
      // Prevents blur when Tree loses focus
      preventBlur: _propTypes.default.bool
    };
  }

  static get defaultProps() {
    return {
      autoExpandDepth: AUTO_EXPAND_DEPTH,
      autoExpandAll: true
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      autoExpanded: new Set()
    };
    this.treeRef = _react.default.createRef();

    const opaf = fn => oncePerAnimationFrame(fn, {
      getDocument: () => this.treeRef.current && this.treeRef.current.ownerDocument
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

  componentWillReceiveProps(nextProps) {
    this._autoExpand();
  }

  componentDidUpdate(prevProps, prevState) {
    if (this.props.focused && prevProps.focused !== this.props.focused) {
      this._scrollNodeIntoView(this.props.focused);
    }
  }

  _autoExpand() {
    const {
      autoExpandDepth,
      autoExpandNodeChildrenLimit,
      initiallyExpanded
    } = this.props;

    if (!autoExpandDepth && !initiallyExpanded) {
      return;
    } // Automatically expand the first autoExpandDepth levels for new items. Do
    // not use the usual DFS infrastructure because we don't want to ignore
    // collapsed nodes. Any initially expanded items will be expanded regardless
    // of how deep they are.


    const autoExpand = (item, currentDepth) => {
      const initial = initiallyExpanded && initiallyExpanded(item);

      if (!initial && currentDepth >= autoExpandDepth) {
        return;
      }

      const children = this.props.getChildren(item);

      if (!initial && autoExpandNodeChildrenLimit && children.length > autoExpandNodeChildrenLimit) {
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
    traversal.push({
      item,
      depth: _depth
    });

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
    const {
      preventAutoScroll
    } = options;

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
        const {
          top,
          bottom
        } = element.getBoundingClientRect();

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
        const scrolledParentRect = scrolledParent ? scrolledParent.getBoundingClientRect() : null;
        const isVisible = !scrolledParent || top >= scrolledParentRect.top && bottom <= scrolledParentRect.bottom;

        if (!isVisible) {
          const {
            alignTo
          } = options;
          const scrollToTop = alignTo ? alignTo === "top" : !scrolledParentRect || top < scrolledParentRect.top;
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
      const {
        relatedTarget
      } = e;

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
    } // Allow parent nodes to use navigation arrows with modifiers.


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
        if (this.props.isExpanded(this.props.focused) && this._nodeIsExpandable(this.props.focused)) {
          this._onCollapse(this.props.focused);
        } else {
          this._focusParentNode();
        }

        return;

      case "ArrowRight":
        if (this._nodeIsExpandable(this.props.focused) && !this.props.isExpanded(this.props.focused)) {
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

    this._focus(prev, {
      alignTo: "top"
    });
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
      this._focus(traversal[i + 1].item, {
        alignTo: "bottom"
      });
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

    this._focus(parent, {
      alignTo: "top"
    });
  }

  _focusFirstNode() {
    const traversal = this._dfsFromRoots();

    this._focus(traversal[0].item, {
      alignTo: "top"
    });
  }

  _focusLastNode() {
    const traversal = this._dfsFromRoots();

    const lastIndex = traversal.length - 1;

    this._focus(traversal[lastIndex].item, {
      alignTo: "bottom"
    });
  }

  _nodeIsExpandable(item) {
    return this.props.isExpandable ? this.props.isExpandable(item) : !!this.props.getChildren(item).length;
  }

  render() {
    const traversal = this._dfsFromRoots();

    const {
      active,
      focused
    } = this.props;
    const nodes = traversal.map((v, i) => {
      const {
        item,
        depth
      } = traversal[i];
      const key = this.props.getKey(item, i);
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
        focused: focused === item,
        active: active === item,
        expanded: this.props.isExpanded(item),
        isExpandable: this._nodeIsExpandable(item),
        onExpand: this._onExpand,
        onCollapse: this._onCollapse,
        onClick: e => {
          // We can stop the propagation since click handler on the node can be
          // created in `renderItem`.
          e.stopPropagation(); // Since the user just clicked the node, there's no need to check if
          // it should be scrolled into view.

          this._focus(item, {
            preventAutoScroll: true
          });

          if (this.props.isExpanded(item)) {
            this.props.onCollapse(item, e.altKey);
          } else {
            this.props.onExpand(item, e.altKey);
          } // Focus should always remain on the tree container itself.


          this.treeRef.current.focus();
        }
      });
    });
    const style = Object.assign({}, this.props.style || {});
    return _reactDomFactories.default.div({
      className: `tree ${this.props.className ? this.props.className : ""}`,
      ref: this.treeRef,
      role: "tree",
      tabIndex: "0",
      onKeyDown: this._onKeyDown,
      onKeyPress: this._preventArrowKeyScrolling,
      onKeyUp: this._preventArrowKeyScrolling,
      onFocus: ({
        nativeEvent
      }) => {
        if (focused || !nativeEvent || !this.treeRef.current) {
          return;
        }

        const {
          explicitOriginalTarget
        } = nativeEvent; // Only set default focus to the first tree node if the focus came
        // from outside the tree (e.g. by tabbing to the tree from other
        // external elements).

        if (explicitOriginalTarget !== this.treeRef.current && !this.treeRef.current.contains(explicitOriginalTarget)) {
          this._focus(traversal[0].item);
        }
      },
      onBlur: this._onBlur,
      "aria-label": this.props.label,
      "aria-labelledby": this.props.labelledby,
      "aria-activedescendant": focused && this.props.getKey(focused),
      style
    }, nodes);
  }

}

var _default = Tree;
exports.default = _default;

/***/ }),

/***/ 110:
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),

/***/ 111:
/***/ (function(module, exports, __webpack_require__) {

(function() {
  var computeScore, countDir, file_coeff, getExtension, getExtensionScore, isMatch, scorePath, scoreSize, tau_depth, _ref;

  _ref = __webpack_require__(66), isMatch = _ref.isMatch, computeScore = _ref.computeScore, scoreSize = _ref.scoreSize;

  tau_depth = 20;

  file_coeff = 2.5;

  exports.score = function(string, query, options) {
    var allowErrors, preparedQuery, score, string_lw;
    preparedQuery = options.preparedQuery, allowErrors = options.allowErrors;
    if (!(allowErrors || isMatch(string, preparedQuery.core_lw, preparedQuery.core_up))) {
      return 0;
    }
    string_lw = string.toLowerCase();
    score = computeScore(string, string_lw, preparedQuery);
    score = scorePath(string, string_lw, score, options);
    return Math.ceil(score);
  };

  scorePath = function(subject, subject_lw, fullPathScore, options) {
    var alpha, basePathScore, basePos, depth, end, extAdjust, fileLength, pathSeparator, preparedQuery, useExtensionBonus;
    if (fullPathScore === 0) {
      return 0;
    }
    preparedQuery = options.preparedQuery, useExtensionBonus = options.useExtensionBonus, pathSeparator = options.pathSeparator;
    end = subject.length - 1;
    while (subject[end] === pathSeparator) {
      end--;
    }
    basePos = subject.lastIndexOf(pathSeparator, end);
    fileLength = end - basePos;
    extAdjust = 1.0;
    if (useExtensionBonus) {
      extAdjust += getExtensionScore(subject_lw, preparedQuery.ext, basePos, end, 2);
      fullPathScore *= extAdjust;
    }
    if (basePos === -1) {
      return fullPathScore;
    }
    depth = preparedQuery.depth;
    while (basePos > -1 && depth-- > 0) {
      basePos = subject.lastIndexOf(pathSeparator, basePos - 1);
    }
    basePathScore = basePos === -1 ? fullPathScore : extAdjust * computeScore(subject.slice(basePos + 1, end + 1), subject_lw.slice(basePos + 1, end + 1), preparedQuery);
    alpha = 0.5 * tau_depth / (tau_depth + countDir(subject, end + 1, pathSeparator));
    return alpha * basePathScore + (1 - alpha) * fullPathScore * scoreSize(0, file_coeff * fileLength);
  };

  exports.countDir = countDir = function(path, end, pathSeparator) {
    var count, i;
    if (end < 1) {
      return 0;
    }
    count = 0;
    i = -1;
    while (++i < end && path[i] === pathSeparator) {
      continue;
    }
    while (++i < end) {
      if (path[i] === pathSeparator) {
        count++;
        while (++i < end && path[i] === pathSeparator) {
          continue;
        }
      }
    }
    return count;
  };

  exports.getExtension = getExtension = function(str) {
    var pos;
    pos = str.lastIndexOf(".");
    if (pos < 0) {
      return "";
    } else {
      return str.substr(pos + 1);
    }
  };

  getExtensionScore = function(candidate, ext, startPos, endPos, maxDepth) {
    var m, matched, n, pos;
    if (!ext.length) {
      return 0;
    }
    pos = candidate.lastIndexOf(".", endPos);
    if (!(pos > startPos)) {
      return 0;
    }
    n = ext.length;
    m = endPos - pos;
    if (m < n) {
      n = m;
      m = ext.length;
    }
    pos++;
    matched = -1;
    while (++matched < n) {
      if (candidate[pos + matched] !== ext[matched]) {
        break;
      }
    }
    if (matched === 0 && maxDepth > 0) {
      return 0.9 * getExtensionScore(candidate, ext, startPos, pos - 2, maxDepth - 1);
    }
    return matched / m;
  };

}).call(this);


/***/ }),

/***/ 112:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_112__;

/***/ }),

/***/ 13:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function networkRequest(url, opts) {
  return fetch(url, {
    cache: opts.loadFromCache ? "default" : "no-cache"
  }).then(res => {
    if (res.status >= 200 && res.status < 300) {
      if (res.headers.get("Content-Type") === "application/wasm") {
        return res.arrayBuffer().then(buffer => ({
          content: buffer,
          isDwarf: true
        }));
      }

      return res.text().then(text => ({
        content: text
      }));
    }

    return Promise.reject(`request failed with status ${res.status}`);
  });
}

module.exports = networkRequest;

/***/ }),

/***/ 14:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function WorkerDispatcher() {
  this.msgId = 1;
  this.worker = null;
}

WorkerDispatcher.prototype = {
  start(url, win = window) {
    this.worker = new win.Worker(url);

    this.worker.onerror = err => {
      console.error(`Error in worker ${url}`, err.message);
    };
  },

  stop() {
    if (!this.worker) {
      return;
    }

    this.worker.terminate();
    this.worker = null;
  },

  task(method, {
    queue = false
  } = {}) {
    const calls = [];

    const push = args => {
      return new Promise((resolve, reject) => {
        if (queue && calls.length === 0) {
          Promise.resolve().then(flush);
        }

        calls.push([args, resolve, reject]);

        if (!queue) {
          flush();
        }
      });
    };

    const flush = () => {
      const items = calls.slice();
      calls.length = 0;

      if (!this.worker) {
        return;
      }

      const id = this.msgId++;
      this.worker.postMessage({
        id,
        method,
        calls: items.map(item => item[0])
      });

      const listener = ({
        data: result
      }) => {
        if (result.id !== id) {
          return;
        }

        if (!this.worker) {
          return;
        }

        this.worker.removeEventListener("message", listener);
        result.results.forEach((resultData, i) => {
          const [, resolve, reject] = items[i];

          if (resultData.error) {
            const err = new Error(resultData.message);
            err.metadata = resultData.metadata;
            reject(err);
          } else {
            resolve(resultData.response);
          }
        });
      };

      this.worker.addEventListener("message", listener);
    };

    return (...args) => push(args);
  },

  invoke(method, ...args) {
    return this.task(method)(...args);
  }

};

function workerHandler(publicInterface) {
  return function (msg) {
    const {
      id,
      method,
      calls
    } = msg.data;
    Promise.all(calls.map(args => {
      try {
        const response = publicInterface[method].apply(undefined, args);

        if (response instanceof Promise) {
          return response.then(val => ({
            response: val
          }), err => asErrorMessage(err));
        }

        return {
          response
        };
      } catch (error) {
        return asErrorMessage(error);
      }
    })).then(results => {
      self.postMessage({
        id,
        results
      });
    });
  };
}

function asErrorMessage(error) {
  if (typeof error === "object" && error && "message" in error) {
    // Error can't be sent via postMessage, so be sure to convert to
    // string.
    return {
      error: true,
      message: error.message,
      metadata: error.metadata
    };
  }

  return {
    error: true,
    message: error == null ? error : error.toString(),
    metadata: undefined
  };
}

module.exports = {
  WorkerDispatcher,
  workerHandler
};

/***/ }),

/***/ 15:
/***/ (function(module, exports) {

var g;

// This works in non-strict mode
g = (function() {
	return this;
})();

try {
	// This works if eval is allowed (see CSP)
	g = g || Function("return this")() || (1,eval)("this");
} catch(e) {
	// This works if the window reference is available
	if(typeof window === "object")
		g = window;
}

// g can still be undefined, but nothing to do about it...
// We return undefined, instead of nothing here, so it's
// easier to handle this case. if(!global) { ...}

module.exports = g;


/***/ }),

/***/ 183:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const {
  PrefsHelper
} = __webpack_require__(424);

const KeyShortcuts = __webpack_require__(425);

const {
  ZoomKeys
} = __webpack_require__(426);

const EventEmitter = __webpack_require__(65);

const asyncStorage = __webpack_require__(427);

const asyncStoreHelper = __webpack_require__(516);

const SourceUtils = __webpack_require__(428);

const Telemetry = __webpack_require__(429);

const {
  getUnicodeHostname,
  getUnicodeUrlPath,
  getUnicodeUrl
} = __webpack_require__(430);

const PluralForm = __webpack_require__(501);

const saveAs = __webpack_require__(520);

module.exports = {
  KeyShortcuts,
  PrefsHelper,
  ZoomKeys,
  asyncStorage,
  asyncStoreHelper,
  EventEmitter,
  SourceUtils,
  Telemetry,
  getUnicodeHostname,
  getUnicodeUrlPath,
  getUnicodeUrl,
  PluralForm,
  saveAs
};

/***/ }),

/***/ 184:
/***/ (function(module, exports, __webpack_require__) {

(function() {
  var Query, coreChars, countDir, getCharCodes, getExtension, opt_char_re, truncatedUpperCase, _ref;

  _ref = __webpack_require__(111), countDir = _ref.countDir, getExtension = _ref.getExtension;

  module.exports = Query = (function() {
    function Query(query, _arg) {
      var optCharRegEx, pathSeparator, _ref1;
      _ref1 = _arg != null ? _arg : {}, optCharRegEx = _ref1.optCharRegEx, pathSeparator = _ref1.pathSeparator;
      if (!(query && query.length)) {
        return null;
      }
      this.query = query;
      this.query_lw = query.toLowerCase();
      this.core = coreChars(query, optCharRegEx);
      this.core_lw = this.core.toLowerCase();
      this.core_up = truncatedUpperCase(this.core);
      this.depth = countDir(query, query.length, pathSeparator);
      this.ext = getExtension(this.query_lw);
      this.charCodes = getCharCodes(this.query_lw);
    }

    return Query;

  })();

  opt_char_re = /[ _\-:\/\\]/g;

  coreChars = function(query, optCharRegEx) {
    if (optCharRegEx == null) {
      optCharRegEx = opt_char_re;
    }
    return query.replace(optCharRegEx, '');
  };

  truncatedUpperCase = function(str) {
    var char, upper, _i, _len;
    upper = "";
    for (_i = 0, _len = str.length; _i < _len; _i++) {
      char = str[_i];
      upper += char.toUpperCase()[0];
    }
    return upper;
  };

  getCharCodes = function(str) {
    var charCodes, i, len;
    len = str.length;
    i = -1;
    charCodes = [];
    while (++i < len) {
      charCodes[str.charCodeAt(i)] = true;
    }
    return charCodes;
  };

}).call(this);


/***/ }),

/***/ 185:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _propTypes = _interopRequireDefault(__webpack_require__(0));

var _react = _interopRequireDefault(__webpack_require__(6));

var _tab = _interopRequireDefault(__webpack_require__(186));

var _tabList = _interopRequireDefault(__webpack_require__(441));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

class TabList extends _react.default.Component {
  constructor(props) {
    super(props);

    const childrenCount = _react.default.Children.count(props.children);

    this.handleKeyPress = this.handleKeyPress.bind(this);
    this.tabRefs = new Array(childrenCount).fill(0).map(() => _react.default.createRef());
    this.handlers = this.getHandlers(props.vertical);
  }

  componentDidUpdate(prevProps) {
    if (prevProps.activeIndex !== this.props.activeIndex) {
      this.tabRefs[this.props.activeIndex].current.focus();
    }
  }

  getHandlers(vertical) {
    if (vertical) {
      return {
        ArrowDown: this.next.bind(this),
        ArrowUp: this.previous.bind(this)
      };
    }

    return {
      ArrowLeft: this.previous.bind(this),
      ArrowRight: this.next.bind(this)
    };
  }

  wrapIndex(index) {
    const count = _react.default.Children.count(this.props.children);

    return (index + count) % count;
  }

  handleKeyPress(event) {
    const handler = this.handlers[event.key];

    if (handler) {
      handler();
    }
  }

  previous() {
    const newIndex = this.wrapIndex(this.props.activeIndex - 1);
    this.props.onActivateTab(newIndex);
  }

  next() {
    const newIndex = this.wrapIndex(this.props.activeIndex + 1);
    this.props.onActivateTab(newIndex);
  }

  render() {
    const {
      accessibleId,
      activeIndex,
      children,
      className,
      onActivateTab
    } = this.props;
    return _react.default.createElement("ul", {
      className: className,
      onKeyUp: this.handleKeyPress,
      role: "tablist"
    }, _react.default.Children.map(children, (child, index) => {
      if (child.type !== _tab.default) {
        throw new Error('Direct children of a <TabList> must be a <Tab>');
      }

      const active = index === activeIndex;
      const tabRef = this.tabRefs[index];
      return _react.default.cloneElement(child, {
        accessibleId: active ? accessibleId : undefined,
        active,
        tabRef,
        onActivate: () => onActivateTab(index)
      });
    }));
  }

}

exports.default = TabList;
TabList.propTypes = {
  accessibleId: _propTypes.default.string,
  activeIndex: _propTypes.default.number,
  children: _propTypes.default.node,
  className: _propTypes.default.string,
  onActivateTab: _propTypes.default.func,
  vertical: _propTypes.default.bool
};
TabList.defaultProps = {
  accessibleId: undefined,
  activeIndex: 0,
  children: null,
  className: _tabList.default.container,
  onActivateTab: () => {},
  vertical: false
};

/***/ }),

/***/ 186:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = Tab;

var _propTypes = _interopRequireDefault(__webpack_require__(0));

var _react = _interopRequireDefault(__webpack_require__(6));

var _ref = _interopRequireDefault(__webpack_require__(439));

var _tab = _interopRequireDefault(__webpack_require__(440));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function Tab({
  accessibleId,
  active,
  children,
  className,
  onActivate,
  tabRef
}) {
  return _react.default.createElement("li", {
    "aria-selected": active,
    className: className,
    id: accessibleId,
    onClick: onActivate,
    onKeyDown: () => {},
    ref: tabRef,
    role: "tab",
    tabIndex: active ? 0 : undefined
  }, children);
}

Tab.propTypes = {
  accessibleId: _propTypes.default.string,
  active: _propTypes.default.bool,
  children: _propTypes.default.node.isRequired,
  className: _propTypes.default.string,
  onActivate: _propTypes.default.func,
  tabRef: _ref.default
};
Tab.defaultProps = {
  accessibleId: undefined,
  active: false,
  className: _tab.default.container,
  onActivate: undefined,
  tabRef: undefined
};

/***/ }),

/***/ 187:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = TabPanels;

var _propTypes = _interopRequireDefault(__webpack_require__(0));

var _react = _interopRequireDefault(__webpack_require__(6));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function TabPanels({
  accessibleId,
  activeIndex,
  children,
  className,
  hasFocusableContent
}) {
  return _react.default.createElement("div", {
    "aria-labelledby": accessibleId,
    role: "tabpanel",
    className: className,
    tabIndex: hasFocusableContent ? undefined : 0
  }, _react.default.Children.toArray(children)[activeIndex]);
}

TabPanels.propTypes = {
  accessibleId: _propTypes.default.string,
  activeIndex: _propTypes.default.number,
  children: _propTypes.default.node.isRequired,
  className: _propTypes.default.string,
  hasFocusableContent: _propTypes.default.bool.isRequired
};
TabPanels.defaultProps = {
  accessibleId: undefined,
  activeIndex: 0,
  className: null
};

/***/ }),

/***/ 22:
/***/ (function(module, exports) {

module.exports = function(module) {
	if(!module.webpackPolyfill) {
		module.deprecate = function() {};
		module.paths = [];
		// module.parent = undefined by default
		if(!module.children) module.children = [];
		Object.defineProperty(module, "loaded", {
			enumerable: true,
			get: function() {
				return module.l;
			}
		});
		Object.defineProperty(module, "id", {
			enumerable: true,
			get: function() {
				return module.i;
			}
		});
		module.webpackPolyfill = 1;
	}
	return module;
};


/***/ }),

/***/ 35:
/***/ (function(module, exports) {

// shim for using process in browser
var process = module.exports = {};

// cached from whatever global is present so that test runners that stub it
// don't break things.  But we need to wrap it in a try catch in case it is
// wrapped in strict mode code which doesn't define any globals.  It's inside a
// function because try/catches deoptimize in certain engines.

var cachedSetTimeout;
var cachedClearTimeout;

function defaultSetTimout() {
    throw new Error('setTimeout has not been defined');
}
function defaultClearTimeout () {
    throw new Error('clearTimeout has not been defined');
}
(function () {
    try {
        if (typeof setTimeout === 'function') {
            cachedSetTimeout = setTimeout;
        } else {
            cachedSetTimeout = defaultSetTimout;
        }
    } catch (e) {
        cachedSetTimeout = defaultSetTimout;
    }
    try {
        if (typeof clearTimeout === 'function') {
            cachedClearTimeout = clearTimeout;
        } else {
            cachedClearTimeout = defaultClearTimeout;
        }
    } catch (e) {
        cachedClearTimeout = defaultClearTimeout;
    }
} ())
function runTimeout(fun) {
    if (cachedSetTimeout === setTimeout) {
        //normal enviroments in sane situations
        return setTimeout(fun, 0);
    }
    // if setTimeout wasn't available but was latter defined
    if ((cachedSetTimeout === defaultSetTimout || !cachedSetTimeout) && setTimeout) {
        cachedSetTimeout = setTimeout;
        return setTimeout(fun, 0);
    }
    try {
        // when when somebody has screwed with setTimeout but no I.E. maddness
        return cachedSetTimeout(fun, 0);
    } catch(e){
        try {
            // When we are in I.E. but the script has been evaled so I.E. doesn't trust the global object when called normally
            return cachedSetTimeout.call(null, fun, 0);
        } catch(e){
            // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error
            return cachedSetTimeout.call(this, fun, 0);
        }
    }


}
function runClearTimeout(marker) {
    if (cachedClearTimeout === clearTimeout) {
        //normal enviroments in sane situations
        return clearTimeout(marker);
    }
    // if clearTimeout wasn't available but was latter defined
    if ((cachedClearTimeout === defaultClearTimeout || !cachedClearTimeout) && clearTimeout) {
        cachedClearTimeout = clearTimeout;
        return clearTimeout(marker);
    }
    try {
        // when when somebody has screwed with setTimeout but no I.E. maddness
        return cachedClearTimeout(marker);
    } catch (e){
        try {
            // When we are in I.E. but the script has been evaled so I.E. doesn't  trust the global object when called normally
            return cachedClearTimeout.call(null, marker);
        } catch (e){
            // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error.
            // Some versions of I.E. have different rules for clearTimeout vs setTimeout
            return cachedClearTimeout.call(this, marker);
        }
    }



}
var queue = [];
var draining = false;
var currentQueue;
var queueIndex = -1;

function cleanUpNextTick() {
    if (!draining || !currentQueue) {
        return;
    }
    draining = false;
    if (currentQueue.length) {
        queue = currentQueue.concat(queue);
    } else {
        queueIndex = -1;
    }
    if (queue.length) {
        drainQueue();
    }
}

function drainQueue() {
    if (draining) {
        return;
    }
    var timeout = runTimeout(cleanUpNextTick);
    draining = true;

    var len = queue.length;
    while(len) {
        currentQueue = queue;
        queue = [];
        while (++queueIndex < len) {
            if (currentQueue) {
                currentQueue[queueIndex].run();
            }
        }
        queueIndex = -1;
        len = queue.length;
    }
    currentQueue = null;
    draining = false;
    runClearTimeout(timeout);
}

process.nextTick = function (fun) {
    var args = new Array(arguments.length - 1);
    if (arguments.length > 1) {
        for (var i = 1; i < arguments.length; i++) {
            args[i - 1] = arguments[i];
        }
    }
    queue.push(new Item(fun, args));
    if (queue.length === 1 && !draining) {
        runTimeout(drainQueue);
    }
};

// v8 likes predictible objects
function Item(fun, array) {
    this.fun = fun;
    this.array = array;
}
Item.prototype.run = function () {
    this.fun.apply(null, this.array);
};
process.title = 'browser';
process.browser = true;
process.env = {};
process.argv = [];
process.version = ''; // empty string to avoid regexp issues
process.versions = {};

function noop() {}

process.on = noop;
process.addListener = noop;
process.once = noop;
process.off = noop;
process.removeListener = noop;
process.removeAllListeners = noop;
process.emit = noop;
process.prependListener = noop;
process.prependOnceListener = noop;

process.listeners = function (name) { return [] }

process.binding = function (name) {
    throw new Error('process.binding is not supported');
};

process.cwd = function () { return '/' };
process.chdir = function (dir) {
    throw new Error('process.chdir is not supported');
};
process.umask = function() { return 0; };


/***/ }),

/***/ 37:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_37__;

/***/ }),

/***/ 413:
/***/ (function(module, exports, __webpack_require__) {

module.exports = __webpack_require__(414);


/***/ }),

/***/ 414:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.vendored = void 0;

var devtoolsComponents = _interopRequireWildcard(__webpack_require__(108));

var devtoolsConfig = _interopRequireWildcard(__webpack_require__(415));

var devtoolsContextmenu = _interopRequireWildcard(__webpack_require__(420));

var devtoolsEnvironment = _interopRequireWildcard(__webpack_require__(102));

var devtoolsModules = _interopRequireWildcard(__webpack_require__(183));

var devtoolsUtils = _interopRequireWildcard(__webpack_require__(7));

var fuzzaldrinPlus = _interopRequireWildcard(__webpack_require__(432));

var transition = _interopRequireWildcard(__webpack_require__(435));

var reactAriaComponentsTabs = _interopRequireWildcard(__webpack_require__(438));

var _classnames = _interopRequireDefault(__webpack_require__(67));

var _devtoolsSplitter = _interopRequireDefault(__webpack_require__(445));

var _lodashMove = _interopRequireDefault(__webpack_require__(449));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Vendors.js is a file used to bundle and expose all dependencies needed to run
 * the transpiled debugger modules when running in Firefox.
 *
 * To make transpilation easier, a vendored module should always be imported in
 * same way:
 * - always with destructuring (import { a } from "modA";)
 * - always without destructuring (import modB from "modB")
 *
 * Both are fine, but cannot be mixed for the same module.
 */
// Modules imported with destructuring
// $FlowIgnore
// Modules imported without destructuring
// We cannot directly export literals containing special characters
// (eg. "my-module/Test") which is why they are nested in "vendored".
// The keys of the vendored object should match the module names
// !!! Should remain synchronized with .babel/transform-mc.js !!!
const vendored = {
  classnames: _classnames.default,
  "devtools-components": devtoolsComponents,
  "devtools-config": devtoolsConfig,
  "devtools-contextmenu": devtoolsContextmenu,
  "devtools-environment": devtoolsEnvironment,
  "devtools-modules": devtoolsModules,
  "devtools-splitter": _devtoolsSplitter.default,
  "devtools-utils": devtoolsUtils,
  "fuzzaldrin-plus": fuzzaldrinPlus,
  "lodash-move": _lodashMove.default,
  "react-aria-components/src/tabs": reactAriaComponentsTabs,
  "react-transition-group/Transition": transition
};
exports.vendored = vendored;

/***/ }),

/***/ 415:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const feature = __webpack_require__(416);

module.exports = feature;

/***/ }),

/***/ 416:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const {
  get: pick,
  set: put
} = __webpack_require__(417);

const fs = __webpack_require__(418);

const path = __webpack_require__(419);

let config;
/**
 * Gets a config value for a given key
 * e.g "chrome.webSocketPort"
 */

function getValue(key) {
  return pick(config, key);
}

function setValue(key, value) {
  return put(config, key, value);
}

function setConfig(value) {
  config = value;
}

function getConfig() {
  return config;
}

function updateLocalConfig(relativePath) {
  const localConfigPath = path.resolve(relativePath, "../configs/local.json");
  const output = JSON.stringify(config, null, 2);
  fs.writeFileSync(localConfigPath, output, {
    flag: "w"
  });
  return output;
}

module.exports = {
  getValue,
  setValue,
  getConfig,
  setConfig,
  updateLocalConfig
};

/***/ }),

/***/ 417:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_417__;

/***/ }),

/***/ 418:
/***/ (function(module, exports) {



/***/ }),

/***/ 419:
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(process) {// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// resolves . and .. elements in a path array with directory names there
// must be no slashes, empty elements, or device names (c:\) in the array
// (so also no leading and trailing slashes - it does not distinguish
// relative and absolute paths)
function normalizeArray(parts, allowAboveRoot) {
  // if the path tries to go above the root, `up` ends up > 0
  var up = 0;
  for (var i = parts.length - 1; i >= 0; i--) {
    var last = parts[i];
    if (last === '.') {
      parts.splice(i, 1);
    } else if (last === '..') {
      parts.splice(i, 1);
      up++;
    } else if (up) {
      parts.splice(i, 1);
      up--;
    }
  }

  // if the path is allowed to go above the root, restore leading ..s
  if (allowAboveRoot) {
    for (; up--; up) {
      parts.unshift('..');
    }
  }

  return parts;
}

// Split a filename into [root, dir, basename, ext], unix version
// 'root' is just a slash, or nothing.
var splitPathRe =
    /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
var splitPath = function(filename) {
  return splitPathRe.exec(filename).slice(1);
};

// path.resolve([from ...], to)
// posix version
exports.resolve = function() {
  var resolvedPath = '',
      resolvedAbsolute = false;

  for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
    var path = (i >= 0) ? arguments[i] : process.cwd();

    // Skip empty and invalid entries
    if (typeof path !== 'string') {
      throw new TypeError('Arguments to path.resolve must be strings');
    } else if (!path) {
      continue;
    }

    resolvedPath = path + '/' + resolvedPath;
    resolvedAbsolute = path.charAt(0) === '/';
  }

  // At this point the path should be resolved to a full absolute path, but
  // handle relative paths to be safe (might happen when process.cwd() fails)

  // Normalize the path
  resolvedPath = normalizeArray(filter(resolvedPath.split('/'), function(p) {
    return !!p;
  }), !resolvedAbsolute).join('/');

  return ((resolvedAbsolute ? '/' : '') + resolvedPath) || '.';
};

// path.normalize(path)
// posix version
exports.normalize = function(path) {
  var isAbsolute = exports.isAbsolute(path),
      trailingSlash = substr(path, -1) === '/';

  // Normalize the path
  path = normalizeArray(filter(path.split('/'), function(p) {
    return !!p;
  }), !isAbsolute).join('/');

  if (!path && !isAbsolute) {
    path = '.';
  }
  if (path && trailingSlash) {
    path += '/';
  }

  return (isAbsolute ? '/' : '') + path;
};

// posix version
exports.isAbsolute = function(path) {
  return path.charAt(0) === '/';
};

// posix version
exports.join = function() {
  var paths = Array.prototype.slice.call(arguments, 0);
  return exports.normalize(filter(paths, function(p, index) {
    if (typeof p !== 'string') {
      throw new TypeError('Arguments to path.join must be strings');
    }
    return p;
  }).join('/'));
};


// path.relative(from, to)
// posix version
exports.relative = function(from, to) {
  from = exports.resolve(from).substr(1);
  to = exports.resolve(to).substr(1);

  function trim(arr) {
    var start = 0;
    for (; start < arr.length; start++) {
      if (arr[start] !== '') break;
    }

    var end = arr.length - 1;
    for (; end >= 0; end--) {
      if (arr[end] !== '') break;
    }

    if (start > end) return [];
    return arr.slice(start, end - start + 1);
  }

  var fromParts = trim(from.split('/'));
  var toParts = trim(to.split('/'));

  var length = Math.min(fromParts.length, toParts.length);
  var samePartsLength = length;
  for (var i = 0; i < length; i++) {
    if (fromParts[i] !== toParts[i]) {
      samePartsLength = i;
      break;
    }
  }

  var outputParts = [];
  for (var i = samePartsLength; i < fromParts.length; i++) {
    outputParts.push('..');
  }

  outputParts = outputParts.concat(toParts.slice(samePartsLength));

  return outputParts.join('/');
};

exports.sep = '/';
exports.delimiter = ':';

exports.dirname = function(path) {
  var result = splitPath(path),
      root = result[0],
      dir = result[1];

  if (!root && !dir) {
    // No dirname whatsoever
    return '.';
  }

  if (dir) {
    // It has a dirname, strip trailing slash
    dir = dir.substr(0, dir.length - 1);
  }

  return root + dir;
};


exports.basename = function(path, ext) {
  var f = splitPath(path)[2];
  // TODO: make this comparison case-insensitive on windows?
  if (ext && f.substr(-1 * ext.length) === ext) {
    f = f.substr(0, f.length - ext.length);
  }
  return f;
};


exports.extname = function(path) {
  return splitPath(path)[3];
};

function filter (xs, f) {
    if (xs.filter) return xs.filter(f);
    var res = [];
    for (var i = 0; i < xs.length; i++) {
        if (f(xs[i], i, xs)) res.push(xs[i]);
    }
    return res;
}

// String.prototype.substr - negative index don't work in IE8
var substr = 'ab'.substr(-1) === 'b'
    ? function (str, start, len) { return str.substr(start, len) }
    : function (str, start, len) {
        if (start < 0) start = str.length + start;
        return str.substr(start, len);
    }
;

/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(35)))

/***/ }),

/***/ 420:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const Menu = __webpack_require__(490);

const MenuItem = __webpack_require__(491);

function inToolbox() {
  try {
    return window.parent.document.documentURI.startsWith("about:devtools-toolbox");
  } catch (e) {
    // If `window` is not available, it's very likely that we are in the toolbox.
    return true;
  }
}

if (!inToolbox()) {
  __webpack_require__(431);
}

function createPopup(doc) {
  let popup = doc.createElement("menupopup");
  popup.className = "landing-popup";

  if (popup.openPopupAtScreen) {
    return popup;
  }

  function preventDefault(e) {
    e.preventDefault();
    e.returnValue = false;
  }

  let mask = document.querySelector("#contextmenu-mask");

  if (!mask) {
    mask = doc.createElement("div");
    mask.id = "contextmenu-mask";
    document.body.appendChild(mask);
  }

  mask.onclick = () => popup.hidePopup();

  popup.openPopupAtScreen = function (clientX, clientY) {
    this.style.setProperty("left", `${clientX}px`);
    this.style.setProperty("top", `${clientY}px`);
    mask = document.querySelector("#contextmenu-mask");
    window.onwheel = preventDefault;
    mask.classList.add("show");
    this.dispatchEvent(new Event("popupshown"));
    this.popupshown;
  };

  popup.hidePopup = function () {
    this.remove();
    mask = document.querySelector("#contextmenu-mask");
    mask.classList.remove("show");
    window.onwheel = null;
  };

  return popup;
}

if (!inToolbox()) {
  Menu.prototype.createPopup = createPopup;
}

function onShown(menu, popup) {
  popup.childNodes.forEach((menuItemNode, i) => {
    let item = menu.items[i];

    if (!item.disabled && item.visible) {
      menuItemNode.onclick = () => {
        item.click();
        popup.hidePopup();
      };

      showSubMenu(item.submenu, menuItemNode, popup);
    }
  });
}

function showMenu(evt, items) {
  if (items.length === 0) {
    return;
  }

  let menu = new Menu();
  items.filter(item => item.visible === undefined || item.visible === true).forEach(item => {
    let menuItem = new MenuItem(item);
    menuItem.submenu = createSubMenu(item.submenu);
    menu.append(menuItem);
  });

  if (inToolbox()) {
    menu.popup(evt.screenX, evt.screenY, window.parent.document);
    return;
  }

  menu.on("open", (_, popup) => onShown(menu, popup));
  menu.popup(evt.clientX, evt.clientY, document);
}

function createSubMenu(subItems) {
  if (subItems) {
    let subMenu = new Menu();
    subItems.forEach(subItem => {
      subMenu.append(new MenuItem(subItem));
    });
    return subMenu;
  }

  return null;
}

function showSubMenu(subMenu, menuItemNode, popup) {
  if (subMenu) {
    let subMenuNode = menuItemNode.querySelector("menupopup");
    let {
      top
    } = menuItemNode.getBoundingClientRect();
    let {
      left,
      width
    } = popup.getBoundingClientRect();
    subMenuNode.style.setProperty("left", `${left + width - 1}px`);
    subMenuNode.style.setProperty("top", `${top}px`);
    let subMenuItemNodes = menuItemNode.querySelector("menupopup:not(.landing-popup)").childNodes;
    subMenuItemNodes.forEach((subMenuItemNode, j) => {
      let subMenuItem = subMenu.items.filter(item => item.visible === undefined || item.visible === true)[j];

      if (!subMenuItem.disabled && subMenuItem.visible) {
        subMenuItemNode.onclick = () => {
          subMenuItem.click();
          popup.hidePopup();
        };
      }
    });
  }
}

function buildMenu(items) {
  return items.map(itm => {
    const hide = typeof itm.hidden === "function" ? itm.hidden() : itm.hidden;
    return hide ? null : itm.item;
  }).filter(itm => itm !== null);
}

module.exports = {
  showMenu,
  buildMenu
};

/***/ }),

/***/ 422:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A sham for https://dxr.mozilla.org/mozilla-central/source/toolkit/modules/Promise.jsm
 */

/**
 * Promise.jsm is mostly the Promise web API with a `defer` method. Just drop this in here,
 * and use the native web API (although building with webpack/babel, it may replace this
 * with it's own version if we want to target environments that do not have `Promise`.
 */
let p = typeof window != "undefined" ? window.Promise : Promise;

p.defer = function defer() {
  var resolve, reject;
  var promise = new Promise(function () {
    resolve = arguments[0];
    reject = arguments[1];
  });
  return {
    resolve: resolve,
    reject: reject,
    promise: promise
  };
};

module.exports = p;

/***/ }),

/***/ 424:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const Services = __webpack_require__(37);

const EventEmitter = __webpack_require__(65);
/**
 * Shortcuts for lazily accessing and setting various preferences.
 * Usage:
 *   let prefs = new Prefs("root.path.to.branch", {
 *     myIntPref: ["Int", "leaf.path.to.my-int-pref"],
 *     myCharPref: ["Char", "leaf.path.to.my-char-pref"],
 *     myJsonPref: ["Json", "leaf.path.to.my-json-pref"],
 *     myFloatPref: ["Float", "leaf.path.to.my-float-pref"]
 *     ...
 *   });
 *
 * Get/set:
 *   prefs.myCharPref = "foo";
 *   let aux = prefs.myCharPref;
 *
 * Observe:
 *   prefs.registerObserver();
 *   prefs.on("pref-changed", (prefName, prefValue) => {
 *     ...
 *   });
 *
 * @param string prefsRoot
 *        The root path to the required preferences branch.
 * @param object prefsBlueprint
 *        An object containing { accessorName: [prefType, prefName, prefDefault] } keys.
 */


function PrefsHelper(prefsRoot = "", prefsBlueprint = {}) {
  EventEmitter.decorate(this);
  let cache = new Map();

  for (let accessorName in prefsBlueprint) {
    let [prefType, prefName, prefDefault] = prefsBlueprint[accessorName];
    map(this, cache, accessorName, prefType, prefsRoot, prefName, prefDefault);
  }

  let observer = makeObserver(this, cache, prefsRoot, prefsBlueprint);

  this.registerObserver = () => observer.register();

  this.unregisterObserver = () => observer.unregister();
}
/**
 * Helper method for getting a pref value.
 *
 * @param Map cache
 * @param string prefType
 * @param string prefsRoot
 * @param string prefName
 * @return any
 */


function get(cache, prefType, prefsRoot, prefName) {
  let cachedPref = cache.get(prefName);

  if (cachedPref !== undefined) {
    return cachedPref;
  }

  let value = Services.prefs["get" + prefType + "Pref"]([prefsRoot, prefName].join("."));
  cache.set(prefName, value);
  return value;
}
/**
 * Helper method for setting a pref value.
 *
 * @param Map cache
 * @param string prefType
 * @param string prefsRoot
 * @param string prefName
 * @param any value
 */


function set(cache, prefType, prefsRoot, prefName, value) {
  Services.prefs["set" + prefType + "Pref"]([prefsRoot, prefName].join("."), value);
  cache.set(prefName, value);
}
/**
 * Maps a property name to a pref, defining lazy getters and setters.
 * Supported types are "Bool", "Char", "Int", "Float" (sugar around "Char"
 * type and casting), and "Json" (which is basically just sugar for "Char"
 * using the standard JSON serializer).
 *
 * @param PrefsHelper self
 * @param Map cache
 * @param string accessorName
 * @param string prefType
 * @param string prefsRoot
 * @param string prefName
 * @param string prefDefault
 * @param array serializer [optional]
 */


function map(self, cache, accessorName, prefType, prefsRoot, prefName, prefDefault, serializer = {
  in: e => e,
  out: e => e
}) {
  if (prefName in self) {
    throw new Error(`Can't use ${prefName} because it overrides a property` + "on the instance.");
  }

  if (prefType == "Json") {
    map(self, cache, accessorName, "String", prefsRoot, prefName, prefDefault, {
      in: JSON.parse,
      out: JSON.stringify
    });
    return;
  }

  if (prefType == "Float") {
    map(self, cache, accessorName, "Char", prefsRoot, prefName, prefDefault, {
      in: Number.parseFloat,
      out: n => n + ""
    });
    return;
  }

  Object.defineProperty(self, accessorName, {
    get: () => {
      try {
        return serializer.in(get(cache, prefType, prefsRoot, prefName));
      } catch (e) {
        if (typeof prefDefault !== 'undefined') {
          return prefDefault;
        }

        throw e;
      }
    },
    set: e => set(cache, prefType, prefsRoot, prefName, serializer.out(e))
  });
}
/**
 * Finds the accessor for the provided pref, based on the blueprint object
 * used in the constructor.
 *
 * @param PrefsHelper self
 * @param object prefsBlueprint
 * @return string
 */


function accessorNameForPref(somePrefName, prefsBlueprint) {
  for (let accessorName in prefsBlueprint) {
    let [, prefName] = prefsBlueprint[accessorName];

    if (somePrefName == prefName) {
      return accessorName;
    }
  }

  return "";
}
/**
 * Creates a pref observer for `self`.
 *
 * @param PrefsHelper self
 * @param Map cache
 * @param string prefsRoot
 * @param object prefsBlueprint
 * @return object
 */


function makeObserver(self, cache, prefsRoot, prefsBlueprint) {
  return {
    register: function () {
      this._branch = Services.prefs.getBranch(prefsRoot + ".");

      this._branch.addObserver("", this);
    },
    unregister: function () {
      this._branch.removeObserver("", this);
    },
    observe: function (subject, topic, prefName) {
      // If this particular pref isn't handled by the blueprint object,
      // even though it's in the specified branch, ignore it.
      let accessorName = accessorNameForPref(prefName, prefsBlueprint);

      if (!(accessorName in self)) {
        return;
      }

      cache.delete(prefName);
      self.emit("pref-changed", accessorName, self[accessorName]);
    }
  };
}

exports.PrefsHelper = PrefsHelper;

/***/ }),

/***/ 425:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const {
  appinfo
} = __webpack_require__(37);

const EventEmitter = __webpack_require__(65);

const isOSX = appinfo.OS === "Darwin"; // List of electron keys mapped to DOM API (DOM_VK_*) key code

const ElectronKeysMapping = {
  "F1": "DOM_VK_F1",
  "F2": "DOM_VK_F2",
  "F3": "DOM_VK_F3",
  "F4": "DOM_VK_F4",
  "F5": "DOM_VK_F5",
  "F6": "DOM_VK_F6",
  "F7": "DOM_VK_F7",
  "F8": "DOM_VK_F8",
  "F9": "DOM_VK_F9",
  "F10": "DOM_VK_F10",
  "F11": "DOM_VK_F11",
  "F12": "DOM_VK_F12",
  "F13": "DOM_VK_F13",
  "F14": "DOM_VK_F14",
  "F15": "DOM_VK_F15",
  "F16": "DOM_VK_F16",
  "F17": "DOM_VK_F17",
  "F18": "DOM_VK_F18",
  "F19": "DOM_VK_F19",
  "F20": "DOM_VK_F20",
  "F21": "DOM_VK_F21",
  "F22": "DOM_VK_F22",
  "F23": "DOM_VK_F23",
  "F24": "DOM_VK_F24",
  "Space": "DOM_VK_SPACE",
  "Backspace": "DOM_VK_BACK_SPACE",
  "Delete": "DOM_VK_DELETE",
  "Insert": "DOM_VK_INSERT",
  "Return": "DOM_VK_RETURN",
  "Enter": "DOM_VK_RETURN",
  "Up": "DOM_VK_UP",
  "Down": "DOM_VK_DOWN",
  "Left": "DOM_VK_LEFT",
  "Right": "DOM_VK_RIGHT",
  "Home": "DOM_VK_HOME",
  "End": "DOM_VK_END",
  "PageUp": "DOM_VK_PAGE_UP",
  "PageDown": "DOM_VK_PAGE_DOWN",
  "Escape": "DOM_VK_ESCAPE",
  "Esc": "DOM_VK_ESCAPE",
  "Tab": "DOM_VK_TAB",
  "VolumeUp": "DOM_VK_VOLUME_UP",
  "VolumeDown": "DOM_VK_VOLUME_DOWN",
  "VolumeMute": "DOM_VK_VOLUME_MUTE",
  "PrintScreen": "DOM_VK_PRINTSCREEN"
};
/**
 * Helper to listen for keyboard events decribed in .properties file.
 *
 * let shortcuts = new KeyShortcuts({
 *   window
 * });
 * shortcuts.on("Ctrl+F", event => {
 *   // `event` is the KeyboardEvent which relates to the key shortcuts
 * });
 *
 * @param DOMWindow window
 *        The window object of the document to listen events from.
 * @param DOMElement target
 *        Optional DOM Element on which we should listen events from.
 *        If omitted, we listen for all events fired on `window`.
 */

function KeyShortcuts({
  window,
  target
}) {
  this.window = window;
  this.target = target || window;
  this.keys = new Map();
  this.eventEmitter = new EventEmitter();
  this.target.addEventListener("keydown", this);
}
/*
 * Parse an electron-like key string and return a normalized object which
 * allow efficient match on DOM key event. The normalized object matches DOM
 * API.
 *
 * @param DOMWindow window
 *        Any DOM Window object, just to fetch its `KeyboardEvent` object
 * @param String str
 *        The shortcut string to parse, following this document:
 *        https://github.com/electron/electron/blob/master/docs/api/accelerator.md
 */


KeyShortcuts.parseElectronKey = function (window, str) {
  let modifiers = str.split("+");
  let key = modifiers.pop();
  let shortcut = {
    ctrl: false,
    meta: false,
    alt: false,
    shift: false,
    // Set for character keys
    key: undefined,
    // Set for non-character keys
    keyCode: undefined
  };

  for (let mod of modifiers) {
    if (mod === "Alt") {
      shortcut.alt = true;
    } else if (["Command", "Cmd"].includes(mod)) {
      shortcut.meta = true;
    } else if (["CommandOrControl", "CmdOrCtrl"].includes(mod)) {
      if (isOSX) {
        shortcut.meta = true;
      } else {
        shortcut.ctrl = true;
      }
    } else if (["Control", "Ctrl"].includes(mod)) {
      shortcut.ctrl = true;
    } else if (mod === "Shift") {
      shortcut.shift = true;
    } else {
      console.error("Unsupported modifier:", mod, "from key:", str);
      return null;
    }
  } // Plus is a special case. It's a character key and shouldn't be matched
  // against a keycode as it is only accessible via Shift/Capslock


  if (key === "Plus") {
    key = "+";
  }

  if (typeof key === "string" && key.length === 1) {
    // Match any single character
    shortcut.key = key.toLowerCase();
  } else if (key in ElectronKeysMapping) {
    // Maps the others manually to DOM API DOM_VK_*
    key = ElectronKeysMapping[key];
    shortcut.keyCode = window.KeyboardEvent[key]; // Used only to stringify the shortcut

    shortcut.keyCodeString = key;
    shortcut.key = key;
  } else {
    console.error("Unsupported key:", key);
    return null;
  }

  return shortcut;
};

KeyShortcuts.stringify = function (shortcut) {
  let list = [];

  if (shortcut.alt) {
    list.push("Alt");
  }

  if (shortcut.ctrl) {
    list.push("Ctrl");
  }

  if (shortcut.meta) {
    list.push("Cmd");
  }

  if (shortcut.shift) {
    list.push("Shift");
  }

  let key;

  if (shortcut.key) {
    key = shortcut.key.toUpperCase();
  } else {
    key = shortcut.keyCodeString;
  }

  list.push(key);
  return list.join("+");
};

KeyShortcuts.prototype = {
  destroy() {
    this.target.removeEventListener("keydown", this);
    this.keys.clear();
  },

  doesEventMatchShortcut(event, shortcut) {
    if (shortcut.meta != event.metaKey) {
      return false;
    }

    if (shortcut.ctrl != event.ctrlKey) {
      return false;
    }

    if (shortcut.alt != event.altKey) {
      return false;
    } // Shift is a special modifier, it may implicitely be required if the
    // expected key is a special character accessible via shift.


    if (shortcut.shift != event.shiftKey && event.key && event.key.match(/[a-zA-Z]/)) {
      return false;
    }

    if (shortcut.keyCode) {
      return event.keyCode == shortcut.keyCode;
    } else if (event.key in ElectronKeysMapping) {
      return ElectronKeysMapping[event.key] === shortcut.key;
    } // get the key from the keyCode if key is not provided.


    let key = event.key || String.fromCharCode(event.keyCode); // For character keys, we match if the final character is the expected one.
    // But for digits we also accept indirect match to please azerty keyboard,
    // which requires Shift to be pressed to get digits.

    return key.toLowerCase() == shortcut.key || shortcut.key.match(/^[0-9]$/) && event.keyCode == shortcut.key.charCodeAt(0);
  },

  handleEvent(event) {
    for (let [key, shortcut] of this.keys) {
      if (this.doesEventMatchShortcut(event, shortcut)) {
        this.eventEmitter.emit(key, event);
      }
    }
  },

  on(key, listener) {
    if (typeof listener !== "function") {
      throw new Error("KeyShortcuts.on() expects a function as " + "second argument");
    }

    if (!this.keys.has(key)) {
      let shortcut = KeyShortcuts.parseElectronKey(this.window, key); // The key string is wrong and we were unable to compute the key shortcut

      if (!shortcut) {
        return;
      }

      this.keys.set(key, shortcut);
    }

    this.eventEmitter.on(key, listener);
  },

  off(key, listener) {
    this.eventEmitter.off(key, listener);
  }

};
module.exports = KeyShortcuts;

/***/ }),

/***/ 426:
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Empty shim for "devtools/client/shared/zoom-keys" module
 *
 * Based on nsIMarkupDocumentViewer.fullZoom API
 * https://developer.mozilla.org/en-US/Firefox/Releases/3/Full_page_zoom
 */

exports.register = function (window) {};

/***/ }),

/***/ 427:
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 *
 * Adapted from https://github.com/mozilla-b2g/gaia/blob/f09993563fb5fec4393eb71816ce76cb00463190/shared/js/async_storage.js
 * (converted to use Promises instead of callbacks).
 *
 * This file defines an asynchronous version of the localStorage API, backed by
 * an IndexedDB database.  It creates a global asyncStorage object that has
 * methods like the localStorage object.
 *
 * To store a value use setItem:
 *
 *   asyncStorage.setItem("key", "value");
 *
 * This returns a promise in case you want confirmation that the value has been stored.
 *
 *  asyncStorage.setItem("key", "newvalue").then(function() {
 *    console.log("new value stored");
 *  });
 *
 * To read a value, call getItem(), but note that you must wait for a promise
 * resolution for the value to be retrieved.
 *
 *  asyncStorage.getItem("key").then(function(value) {
 *    console.log("The value of key is:", value);
 *  });
 *
 * Note that unlike localStorage, asyncStorage does not allow you to store and
 * retrieve values by setting and querying properties directly. You cannot just
 * write asyncStorage.key; you have to explicitly call setItem() or getItem().
 *
 * removeItem(), clear(), length(), and key() are like the same-named methods of
 * localStorage, and all return a promise.
 *
 * The asynchronous nature of getItem() makes it tricky to retrieve multiple
 * values. But unlike localStorage, asyncStorage does not require the values you
 * store to be strings.  So if you need to save multiple values and want to
 * retrieve them together, in a single asynchronous operation, just group the
 * values into a single object. The properties of this object may not include
 * DOM elements, but they may include things like Blobs and typed arrays.
 *
 */


const DBNAME = "devtools-async-storage";
const DBVERSION = 1;
const STORENAME = "keyvaluepairs";
var db = null;

function withStore(type, onsuccess, onerror) {
  if (db) {
    const transaction = db.transaction(STORENAME, type);
    const store = transaction.objectStore(STORENAME);
    onsuccess(store);
  } else {
    const openreq = indexedDB.open(DBNAME, DBVERSION);

    openreq.onerror = function withStoreOnError() {
      onerror();
    };

    openreq.onupgradeneeded = function withStoreOnUpgradeNeeded() {
      // First time setup: create an empty object store
      openreq.result.createObjectStore(STORENAME);
    };

    openreq.onsuccess = function withStoreOnSuccess() {
      db = openreq.result;
      const transaction = db.transaction(STORENAME, type);
      const store = transaction.objectStore(STORENAME);
      onsuccess(store);
    };
  }
}

function getItem(itemKey) {
  return new Promise((resolve, reject) => {
    let req;
    withStore("readonly", store => {
      store.transaction.oncomplete = function onComplete() {
        let value = req.result;

        if (value === undefined) {
          value = null;
        }

        resolve(value);
      };

      req = store.get(itemKey);

      req.onerror = function getItemOnError() {
        reject("Error in asyncStorage.getItem(): ", req.error.name);
      };
    }, reject);
  });
}

function setItem(itemKey, value) {
  return new Promise((resolve, reject) => {
    withStore("readwrite", store => {
      store.transaction.oncomplete = resolve;
      const req = store.put(value, itemKey);

      req.onerror = function setItemOnError() {
        reject("Error in asyncStorage.setItem(): ", req.error.name);
      };
    }, reject);
  });
}

function removeItem(itemKey) {
  return new Promise((resolve, reject) => {
    withStore("readwrite", store => {
      store.transaction.oncomplete = resolve;
      const req = store.delete(itemKey);

      req.onerror = function removeItemOnError() {
        reject("Error in asyncStorage.removeItem(): ", req.error.name);
      };
    }, reject);
  });
}

function clear() {
  return new Promise((resolve, reject) => {
    withStore("readwrite", store => {
      store.transaction.oncomplete = resolve;
      const req = store.clear();

      req.onerror = function clearOnError() {
        reject("Error in asyncStorage.clear(): ", req.error.name);
      };
    }, reject);
  });
}

function length() {
  return new Promise((resolve, reject) => {
    let req;
    withStore("readonly", store => {
      store.transaction.oncomplete = function onComplete() {
        resolve(req.result);
      };

      req = store.count();

      req.onerror = function lengthOnError() {
        reject("Error in asyncStorage.length(): ", req.error.name);
      };
    }, reject);
  });
}

function key(n) {
  return new Promise((resolve, reject) => {
    if (n < 0) {
      resolve(null);
      return;
    }

    let req;
    withStore("readonly", store => {
      store.transaction.oncomplete = function onComplete() {
        const cursor = req.result;
        resolve(cursor ? cursor.key : null);
      };

      let advanced = false;
      req = store.openCursor();

      req.onsuccess = function keyOnSuccess() {
        const cursor = req.result;

        if (!cursor) {
          // this means there weren"t enough keys
          return;
        }

        if (n === 0 || advanced) {
          // Either 1) we have the first key, return it if that's what they
          // wanted, or 2) we"ve got the nth key.
          return;
        } // Otherwise, ask the cursor to skip ahead n records


        advanced = true;
        cursor.advance(n);
      };

      req.onerror = function keyOnError() {
        reject("Error in asyncStorage.key(): ", req.error.name);
      };
    }, reject);
  });
}

exports.getItem = getItem;
exports.setItem = setItem;
exports.removeItem = removeItem;
exports.clear = clear;
exports.length = length;
exports.key = key;

/***/ }),

/***/ 428:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// TODO : Localize this (was l10n.getStr("frame.unknownSource"))
const UNKNOWN_SOURCE_STRING = "(unknown)"; // Character codes used in various parsing helper functions.

const CHAR_CODE_A = "a".charCodeAt(0);
const CHAR_CODE_B = "b".charCodeAt(0);
const CHAR_CODE_C = "c".charCodeAt(0);
const CHAR_CODE_D = "d".charCodeAt(0);
const CHAR_CODE_E = "e".charCodeAt(0);
const CHAR_CODE_F = "f".charCodeAt(0);
const CHAR_CODE_H = "h".charCodeAt(0);
const CHAR_CODE_I = "i".charCodeAt(0);
const CHAR_CODE_J = "j".charCodeAt(0);
const CHAR_CODE_L = "l".charCodeAt(0);
const CHAR_CODE_M = "m".charCodeAt(0);
const CHAR_CODE_N = "n".charCodeAt(0);
const CHAR_CODE_O = "o".charCodeAt(0);
const CHAR_CODE_P = "p".charCodeAt(0);
const CHAR_CODE_R = "r".charCodeAt(0);
const CHAR_CODE_S = "s".charCodeAt(0);
const CHAR_CODE_T = "t".charCodeAt(0);
const CHAR_CODE_U = "u".charCodeAt(0);
const CHAR_CODE_W = "w".charCodeAt(0);
const CHAR_CODE_COLON = ":".charCodeAt(0);
const CHAR_CODE_DASH = "-".charCodeAt(0);
const CHAR_CODE_L_SQUARE_BRACKET = "[".charCodeAt(0);
const CHAR_CODE_SLASH = "/".charCodeAt(0);
const CHAR_CODE_CAP_S = "S".charCodeAt(0); // The cache used in the `parseURL` function.

const gURLStore = new Map(); // The cache used in the `getSourceNames` function.

const gSourceNamesStore = new Map();
/**
* Takes a string and returns an object containing all the properties
* available on an URL instance, with additional properties (fileName),
* Leverages caching.
*
* @param {String} location
* @return {Object?} An object containing most properties available
*                   in https://developer.mozilla.org/en-US/docs/Web/API/URL
*/

function parseURL(location) {
  let url = gURLStore.get(location);

  if (url !== void 0) {
    return url;
  }

  try {
    url = new URL(location); // The callers were generally written to expect a URL from
    // sdk/url, which is subtly different.  So, work around some
    // important differences here.

    url = {
      href: url.href,
      protocol: url.protocol,
      host: url.host,
      hostname: url.hostname,
      port: url.port || null,
      pathname: url.pathname,
      search: url.search,
      hash: url.hash,
      username: url.username,
      password: url.password,
      origin: url.origin
    }; // Definitions:
    // Example: https://foo.com:8888/file.js
    // `hostname`: "foo.com"
    // `host`: "foo.com:8888"

    let isChrome = isChromeScheme(location);
    url.fileName = url.pathname ? url.pathname.slice(url.pathname.lastIndexOf("/") + 1) || "/" : "/";

    if (isChrome) {
      url.hostname = null;
      url.host = null;
    }

    gURLStore.set(location, url);
    return url;
  } catch (e) {
    gURLStore.set(location, null);
    return null;
  }
}
/**
* Parse a source into a short and long name as well as a host name.
*
* @param {String} source
*        The source to parse. Can be a URI or names like "(eval)" or
*        "self-hosted".
* @return {Object}
*         An object with the following properties:
*           - {String} short: A short name for the source.
*             - "http://page.com/test.js#go?q=query" -> "test.js"
*           - {String} long: The full, long name for the source, with
              hash/query stripped.
*             - "http://page.com/test.js#go?q=query" -> "http://page.com/test.js"
*           - {String?} host: If available, the host name for the source.
*             - "http://page.com/test.js#go?q=query" -> "page.com"
*/


function getSourceNames(source) {
  let data = gSourceNamesStore.get(source);

  if (data) {
    return data;
  }

  let short, long, host;
  const sourceStr = source ? String(source) : ""; // If `data:...` uri

  if (isDataScheme(sourceStr)) {
    let commaIndex = sourceStr.indexOf(",");

    if (commaIndex > -1) {
      // The `short` name for a data URI becomes `data:` followed by the actual
      // encoded content, omitting the MIME type, and charset.
      short = `data:${sourceStr.substring(commaIndex + 1)}`.slice(0, 100);
      let result = {
        short,
        long: sourceStr
      };
      gSourceNamesStore.set(source, result);
      return result;
    }
  }

  const parsedUrl = parseURL(sourceStr);

  if (!parsedUrl) {
    // Malformed URI.
    long = sourceStr;
    short = sourceStr.slice(0, 100);
  } else {
    host = parsedUrl.host;
    long = parsedUrl.href;

    if (parsedUrl.hash) {
      long = long.replace(parsedUrl.hash, "");
    }

    if (parsedUrl.search) {
      long = long.replace(parsedUrl.search, "");
    }

    short = parsedUrl.fileName; // If `short` is just a slash, and we actually have a path,
    // strip the slash and parse again to get a more useful short name.
    // e.g. "http://foo.com/bar/" -> "bar", rather than "/"

    if (short === "/" && parsedUrl.pathname !== "/") {
      short = parseURL(long.replace(/\/$/, "")).fileName;
    }
  }

  if (!short) {
    if (!long) {
      long = UNKNOWN_SOURCE_STRING;
    }

    short = long.slice(0, 100);
  }

  let result = {
    short,
    long,
    host
  };
  gSourceNamesStore.set(source, result);
  return result;
} // For the functions below, we assume that we will never access the location
// argument out of bounds, which is indeed the vast majority of cases.
//
// They are written this way because they are hot. Each frame is checked for
// being content or chrome when processing the profile.


function isColonSlashSlash(location, i = 0) {
  return location.charCodeAt(++i) === CHAR_CODE_COLON && location.charCodeAt(++i) === CHAR_CODE_SLASH && location.charCodeAt(++i) === CHAR_CODE_SLASH;
}

function isDataScheme(location, i = 0) {
  return location.charCodeAt(i) === CHAR_CODE_D && location.charCodeAt(++i) === CHAR_CODE_A && location.charCodeAt(++i) === CHAR_CODE_T && location.charCodeAt(++i) === CHAR_CODE_A && location.charCodeAt(++i) === CHAR_CODE_COLON;
}

function isContentScheme(location, i = 0) {
  let firstChar = location.charCodeAt(i);

  switch (firstChar) {
    // "http://" or "https://"
    case CHAR_CODE_H:
      if (location.charCodeAt(++i) === CHAR_CODE_T && location.charCodeAt(++i) === CHAR_CODE_T && location.charCodeAt(++i) === CHAR_CODE_P) {
        if (location.charCodeAt(i + 1) === CHAR_CODE_S) {
          ++i;
        }

        return isColonSlashSlash(location, i);
      }

      return false;
    // "file://"

    case CHAR_CODE_F:
      if (location.charCodeAt(++i) === CHAR_CODE_I && location.charCodeAt(++i) === CHAR_CODE_L && location.charCodeAt(++i) === CHAR_CODE_E) {
        return isColonSlashSlash(location, i);
      }

      return false;
    // "app://"

    case CHAR_CODE_A:
      if (location.charCodeAt(++i) == CHAR_CODE_P && location.charCodeAt(++i) == CHAR_CODE_P) {
        return isColonSlashSlash(location, i);
      }

      return false;
    // "blob:"

    case CHAR_CODE_B:
      if (location.charCodeAt(++i) == CHAR_CODE_L && location.charCodeAt(++i) == CHAR_CODE_O && location.charCodeAt(++i) == CHAR_CODE_B && location.charCodeAt(++i) == CHAR_CODE_COLON) {
        return isContentScheme(location, i + 1);
      }

      return false;

    default:
      return false;
  }
}

function isChromeScheme(location, i = 0) {
  let firstChar = location.charCodeAt(i);

  switch (firstChar) {
    // "chrome://"
    case CHAR_CODE_C:
      if (location.charCodeAt(++i) === CHAR_CODE_H && location.charCodeAt(++i) === CHAR_CODE_R && location.charCodeAt(++i) === CHAR_CODE_O && location.charCodeAt(++i) === CHAR_CODE_M && location.charCodeAt(++i) === CHAR_CODE_E) {
        return isColonSlashSlash(location, i);
      }

      return false;
    // "resource://"

    case CHAR_CODE_R:
      if (location.charCodeAt(++i) === CHAR_CODE_E && location.charCodeAt(++i) === CHAR_CODE_S && location.charCodeAt(++i) === CHAR_CODE_O && location.charCodeAt(++i) === CHAR_CODE_U && location.charCodeAt(++i) === CHAR_CODE_R && location.charCodeAt(++i) === CHAR_CODE_C && location.charCodeAt(++i) === CHAR_CODE_E) {
        return isColonSlashSlash(location, i);
      }

      return false;
    // "jar:file://"

    case CHAR_CODE_J:
      if (location.charCodeAt(++i) === CHAR_CODE_A && location.charCodeAt(++i) === CHAR_CODE_R && location.charCodeAt(++i) === CHAR_CODE_COLON && location.charCodeAt(++i) === CHAR_CODE_F && location.charCodeAt(++i) === CHAR_CODE_I && location.charCodeAt(++i) === CHAR_CODE_L && location.charCodeAt(++i) === CHAR_CODE_E) {
        return isColonSlashSlash(location, i);
      }

      return false;

    default:
      return false;
  }
}

function isWASM(location, i = 0) {
  return (// "wasm-function["
    location.charCodeAt(i) === CHAR_CODE_W && location.charCodeAt(++i) === CHAR_CODE_A && location.charCodeAt(++i) === CHAR_CODE_S && location.charCodeAt(++i) === CHAR_CODE_M && location.charCodeAt(++i) === CHAR_CODE_DASH && location.charCodeAt(++i) === CHAR_CODE_F && location.charCodeAt(++i) === CHAR_CODE_U && location.charCodeAt(++i) === CHAR_CODE_N && location.charCodeAt(++i) === CHAR_CODE_C && location.charCodeAt(++i) === CHAR_CODE_T && location.charCodeAt(++i) === CHAR_CODE_I && location.charCodeAt(++i) === CHAR_CODE_O && location.charCodeAt(++i) === CHAR_CODE_N && location.charCodeAt(++i) === CHAR_CODE_L_SQUARE_BRACKET
  );
}
/**
* A utility method to get the file name from a sourcemapped location
* The sourcemap location can be in any form. This method returns a
* formatted file name for different cases like Windows or OSX.
* @param source
* @returns String
*/


function getSourceMappedFile(source) {
  // If sourcemapped source is a OSX path, return
  // the characters after last "/".
  // If sourcemapped source is a Windowss path, return
  // the characters after last "\\".
  if (source.lastIndexOf("/") >= 0) {
    source = source.slice(source.lastIndexOf("/") + 1);
  } else if (source.lastIndexOf("\\") >= 0) {
    source = source.slice(source.lastIndexOf("\\") + 1);
  }

  return source;
}

module.exports = {
  parseURL,
  getSourceNames,
  isChromeScheme,
  isContentScheme,
  isWASM,
  isDataScheme,
  getSourceMappedFile
};

/***/ }),

/***/ 429:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This is a stub of the DevTools telemetry module and will be replaced by the
 * full version of the file by Webpack for running inside Firefox.
 */
class Telemetry {
  /**
   * Time since the system wide epoch. This is not a monotonic timer but
   * can be used across process boundaries.
   */
  get msSystemNow() {
    return 0;
  }
  /**
   * Starts a timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {Object} obj
   *        Optional parameter. If specified, the timer is associated with this
   *        object, meaning that multiple timers for the same histogram may be
   *        run concurrently, as long as they are associated with different
   *        objects.
   *
   * @returns {Boolean}
   *          True if the timer was successfully started, false otherwise. If a
   *          timer already exists, it can't be started again, and the existing
   *          one will be cleared in order to avoid measurements errors.
   */


  start(histogramId, obj) {
    return true;
  }
  /**
   * Starts a timer associated with a keyed telemetry histogram. The timer can
   * be directly associated with a histogram and its key. Similarly to
   * TelemetryStopwatch.start the histogram and its key can be associated
   * with an object. Each key may have multiple associated objects and each
   * object can be associated with multiple keys.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {String} key
   *        A string which must be a valid histgram key.
   * @param {Object} obj
   *        Optional parameter. If specified, the timer is associated with this
   *        object, meaning that multiple timers for the same histogram may be
   *        run concurrently,as long as they are associated with different
   *        objects.
   *
   * @returns {Boolean}
   *          True if the timer was successfully started, false otherwise. If a
   *          timer already exists, it can't be started again, and the existing
   *          one will be cleared in order to avoid measurements errors.
   */


  startKeyed(histogramId, key, obj) {
    return true;
  }
  /**
   * Stops the timer associated with the given histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the histogram.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {Object} obj
   *        Optional parameter which associates the histogram timer with the
   *        given object.
   * @param {Boolean} canceledOkay
   *        Optional parameter which will suppress any warnings that normally
   *        fire when a stopwatch is finished after being cancelled.
   *        Defaults to false.
   *
   * @returns {Boolean}
   *          True if the timer was succesfully stopped and the data was added
   *          to the histogram, False otherwise.
   */


  finish(histogramId, obj, canceledOkay) {
    return true;
  }
  /**
   * Stops the timer associated with the given keyed histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the keyed histogram.
   *
   * @param {String} histogramId
   *        A string which must be a valid histogram name.
   * @param {String} key
   *        A string which must be a valid histogram key.
   * @param {Object} obj
   *        Optional parameter which associates the histogram timer with the
   *        given object.
   * @param {Boolean} canceledOkay
   *        Optional parameter which will suppress any warnings that normally
   *        fire when a stopwatch is finished after being cancelled.
   *        Defaults to false.
   *
   * @returns {Boolean}
   *          True if the timer was succesfully stopped and the data was added
   *          to the histogram, False otherwise.
   */


  finishKeyed(histogramId, key, obj, cancelledOkay) {
    return true;
  }
  /**
   * Log a value to a histogram.
   *
   * @param  {String} histogramId
   *         Histogram in which the data is to be stored.
   */


  getHistogramById(histogramId) {
    return {
      add: () => {}
    };
  }
  /**
   * Get a keyed histogram.
   *
   * @param  {String} histogramId
   *         Histogram in which the data is to be stored.
   */


  getKeyedHistogramById(histogramId) {
    return {
      add: () => {}
    };
  }
  /**
   * Log a value to a scalar.
   *
   * @param  {String} scalarId
   *         Scalar in which the data is to be stored.
   * @param  value
   *         Value to store.
   */


  scalarSet(scalarId, value) {}
  /**
   * Log a value to a count scalar.
   *
   * @param  {String} scalarId
   *         Scalar in which the data is to be stored.
   * @param  value
   *         Value to store.
   */


  scalarAdd(scalarId, value) {}
  /**
   * Log a value to a keyed count scalar.
   *
   * @param  {String} scalarId
   *         Scalar in which the data is to be stored.
   * @param  {String} key
   *         The key within the  scalar.
   * @param  value
   *         Value to store.
   */


  keyedScalarAdd(scalarId, key, value) {}
  /**
   * Event telemetry is disabled by default. Use this method to enable it for
   * a particular category.
   *
   * @param {Boolean} enabled
   *        Enabled: true or false.
   */


  setEventRecordingEnabled(enabled) {
    return enabled;
  }
  /**
   * Telemetry events often need to make use of a number of properties from
   * completely different codepaths. To make this possible we create a
   * "pending event" along with an array of property names that we need to wait
   * for before sending the event.
   *
   * As each property is received via addEventProperty() we check if all
   * properties have been received. Once they have all been received we send the
   * telemetry event.
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {Array} expected
   *        An array of the properties needed before sending the telemetry
   *        event e.g.
   *        [
   *          "host",
   *          "width"
   *        ]
   */


  preparePendingEvent(obj, method, object, value, expected = []) {}
  /**
   * Adds an expected property for either a current or future pending event.
   * This means that if preparePendingEvent() is called before or after sending
   * the event properties they will automatically added to the event.
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {String} pendingPropName
   *        The pending property name
   * @param {String} pendingPropValue
   *        The pending property value
   */


  addEventProperty(obj, method, object, value, pendingPropName, pendingPropValue) {}
  /**
   * Adds expected properties for either a current or future pending event.
   * This means that if preparePendingEvent() is called before or after sending
   * the event properties they will automatically added to the event.
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {String} pendingObject
   *        An object containing key, value pairs that should be added to the
   *        event as properties.
   */


  addEventProperties(obj, method, object, value, pendingObject) {}
  /**
   * A private method that is not to be used externally. This method is used to
   * prepare a pending telemetry event for sending and then send it via
   * recordEvent().
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   */


  _sendPendingEvent(obj, method, object, value) {}
  /**
   * Send a telemetry event.
   *
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {Object} extra
   *        The telemetry event extra object containing the properties that will
   *        be sent with the event e.g.
   *        {
   *          host: "bottom",
   *          width: "1024"
   *        }
   */


  recordEvent(method, object, value, extra) {}
  /**
   * Sends telemetry pings to indicate that a tool has been opened.
   *
   * @param {String} id
   *        The ID of the tool opened.
   * @param {String} sessionId
   *        Toolbox session id used when we need to ensure a tool really has a
   *        timer before calculating a delta.
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   */


  toolOpened(id, sessionId, obj) {}
  /**
   * Sends telemetry pings to indicate that a tool has been closed.
   *
   * @param {String} id
   *        The ID of the tool opened.
   */


  toolClosed(id, sessionId, obj) {}

}

module.exports = Telemetry;

/***/ }),

/***/ 430:
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// This file is a chrome-API-free version of the module
// devtools/client/shared/unicode-url.js in the mozilla-central repository, so
// that it can be used in Chrome-API-free applications, such as the Launchpad.
// But because of this, it cannot take advantage of utilizing chrome APIs and
// should implement the similar functionalities on its own.
//
// Please keep in mind that if the feature in this file has changed, don't
// forget to also change that accordingly in
// devtools/client/shared/unicode-url.js in the mozilla-central repository.


const punycode = __webpack_require__(521);
/**
 * Gets a readble Unicode hostname from a hostname.
 *
 * If the `hostname` is a readable ASCII hostname, such as example.org, then
 * this function will simply return the original `hostname`.
 *
 * If the `hostname` is a Punycode hostname representing a Unicode domain name,
 * such as xn--g6w.xn--8pv, then this function will return the readable Unicode
 * domain name by decoding the Punycode hostname.
 *
 * @param {string}  hostname
 *                  the hostname from which the Unicode hostname will be
 *                  parsed, such as example.org, xn--g6w.xn--8pv.
 * @return {string} The Unicode hostname. It may be the same as the `hostname`
 *                  passed to this function if the `hostname` itself is
 *                  a readable ASCII hostname or a Unicode hostname.
 */


function getUnicodeHostname(hostname) {
  try {
    return punycode.toUnicode(hostname);
  } catch (err) {}

  return hostname;
}
/**
 * Gets a readble Unicode URL pathname from a URL pathname.
 *
 * If the `urlPath` is a readable ASCII URL pathname, such as /a/b/c.js, then
 * this function will simply return the original `urlPath`.
 *
 * If the `urlPath` is a URI-encoded pathname, such as %E8%A9%A6/%E6%B8%AC.js,
 * then this function will return the readable Unicode pathname.
 *
 * If the `urlPath` is a malformed URL pathname, then this function will simply
 * return the original `urlPath`.
 *
 * @param {string}  urlPath
 *                  the URL path from which the Unicode URL path will be parsed,
 *                  such as /a/b/c.js, %E8%A9%A6/%E6%B8%AC.js.
 * @return {string} The Unicode URL Path. It may be the same as the `urlPath`
 *                  passed to this function if the `urlPath` itself is a readable
 *                  ASCII url or a Unicode url.
 */


function getUnicodeUrlPath(urlPath) {
  try {
    return decodeURIComponent(urlPath);
  } catch (err) {}

  return urlPath;
}
/**
 * Gets a readable Unicode URL from a URL.
 *
 * If the `url` is a readable ASCII URL, such as http://example.org/a/b/c.js,
 * then this function will simply return the original `url`.
 *
 * If the `url` includes either an unreadable Punycode domain name or an
 * unreadable URI-encoded pathname, such as
 * http://xn--g6w.xn--8pv/%E8%A9%A6/%E6%B8%AC.js, then this function will return
 * the readable URL by decoding all its unreadable URL components to Unicode
 * characters.
 *
 * If the `url` is a malformed URL, then this function will return the original
 * `url`.
 *
 * If the `url` is a data: URI, then this function will return the original
 * `url`.
 *
 * @param {string}  url
 *                  the full URL, or a data: URI. from which the readable URL
 *                  will be parsed, such as, http://example.org/a/b/c.js,
 *                  http://xn--g6w.xn--8pv/%E8%A9%A6/%E6%B8%AC.js
 * @return {string} The readable URL. It may be the same as the `url` passed to
 *                  this function if the `url` itself is readable.
 */


function getUnicodeUrl(url) {
  try {
    const {
      protocol,
      hostname
    } = new URL(url);

    if (protocol === "data:") {
      // Never convert a data: URI.
      return url;
    }

    const readableHostname = getUnicodeHostname(hostname);
    url = decodeURIComponent(url);
    return url.replace(hostname, readableHostname);
  } catch (err) {}

  return url;
}

module.exports = {
  getUnicodeHostname,
  getUnicodeUrlPath,
  getUnicodeUrl
};

/***/ }),

/***/ 431:
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),

/***/ 432:
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(process) {(function() {
  var Query, defaultPathSeparator, filter, matcher, parseOptions, pathScorer, preparedQueryCache, scorer;

  filter = __webpack_require__(433);

  matcher = __webpack_require__(434);

  scorer = __webpack_require__(66);

  pathScorer = __webpack_require__(111);

  Query = __webpack_require__(184);

  preparedQueryCache = null;

  defaultPathSeparator = (typeof process !== "undefined" && process !== null ? process.platform : void 0) === "win32" ? '\\' : '/';

  module.exports = {
    filter: function(candidates, query, options) {
      if (options == null) {
        options = {};
      }
      if (!((query != null ? query.length : void 0) && (candidates != null ? candidates.length : void 0))) {
        return [];
      }
      options = parseOptions(options, query);
      return filter(candidates, query, options);
    },
    score: function(string, query, options) {
      if (options == null) {
        options = {};
      }
      if (!((string != null ? string.length : void 0) && (query != null ? query.length : void 0))) {
        return 0;
      }
      options = parseOptions(options, query);
      if (options.usePathScoring) {
        return pathScorer.score(string, query, options);
      } else {
        return scorer.score(string, query, options);
      }
    },
    match: function(string, query, options) {
      var _i, _ref, _results;
      if (options == null) {
        options = {};
      }
      if (!string) {
        return [];
      }
      if (!query) {
        return [];
      }
      if (string === query) {
        return (function() {
          _results = [];
          for (var _i = 0, _ref = string.length; 0 <= _ref ? _i < _ref : _i > _ref; 0 <= _ref ? _i++ : _i--){ _results.push(_i); }
          return _results;
        }).apply(this);
      }
      options = parseOptions(options, query);
      return matcher.match(string, query, options);
    },
    wrap: function(string, query, options) {
      if (options == null) {
        options = {};
      }
      if (!string) {
        return [];
      }
      if (!query) {
        return [];
      }
      options = parseOptions(options, query);
      return matcher.wrap(string, query, options);
    },
    prepareQuery: function(query, options) {
      if (options == null) {
        options = {};
      }
      options = parseOptions(options, query);
      return options.preparedQuery;
    }
  };

  parseOptions = function(options, query) {
    if (options.allowErrors == null) {
      options.allowErrors = false;
    }
    if (options.usePathScoring == null) {
      options.usePathScoring = true;
    }
    if (options.useExtensionBonus == null) {
      options.useExtensionBonus = false;
    }
    if (options.pathSeparator == null) {
      options.pathSeparator = defaultPathSeparator;
    }
    if (options.optCharRegEx == null) {
      options.optCharRegEx = null;
    }
    if (options.wrap == null) {
      options.wrap = null;
    }
    if (options.preparedQuery == null) {
      options.preparedQuery = preparedQueryCache && preparedQueryCache.query === query ? preparedQueryCache : (preparedQueryCache = new Query(query, options));
    }
    return options;
  };

}).call(this);

/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(35)))

/***/ }),

/***/ 433:
/***/ (function(module, exports, __webpack_require__) {

(function() {
  var Query, pathScorer, pluckCandidates, scorer, sortCandidates;

  scorer = __webpack_require__(66);

  pathScorer = __webpack_require__(111);

  Query = __webpack_require__(184);

  pluckCandidates = function(a) {
    return a.candidate;
  };

  sortCandidates = function(a, b) {
    return b.score - a.score;
  };

  module.exports = function(candidates, query, options) {
    var bKey, candidate, key, maxInners, maxResults, score, scoreProvider, scoredCandidates, spotLeft, string, usePathScoring, _i, _len;
    scoredCandidates = [];
    key = options.key, maxResults = options.maxResults, maxInners = options.maxInners, usePathScoring = options.usePathScoring;
    spotLeft = (maxInners != null) && maxInners > 0 ? maxInners : candidates.length + 1;
    bKey = key != null;
    scoreProvider = usePathScoring ? pathScorer : scorer;
    for (_i = 0, _len = candidates.length; _i < _len; _i++) {
      candidate = candidates[_i];
      string = bKey ? candidate[key] : candidate;
      if (!string) {
        continue;
      }
      score = scoreProvider.score(string, query, options);
      if (score > 0) {
        scoredCandidates.push({
          candidate: candidate,
          score: score
        });
        if (!--spotLeft) {
          break;
        }
      }
    }
    scoredCandidates.sort(sortCandidates);
    candidates = scoredCandidates.map(pluckCandidates);
    if (maxResults != null) {
      candidates = candidates.slice(0, maxResults);
    }
    return candidates;
  };

}).call(this);


/***/ }),

/***/ 434:
/***/ (function(module, exports, __webpack_require__) {

(function() {
  var basenameMatch, computeMatch, isMatch, isWordStart, match, mergeMatches, scoreAcronyms, scoreCharacter, scoreConsecutives, _ref;

  _ref = __webpack_require__(66), isMatch = _ref.isMatch, isWordStart = _ref.isWordStart, scoreConsecutives = _ref.scoreConsecutives, scoreCharacter = _ref.scoreCharacter, scoreAcronyms = _ref.scoreAcronyms;

  exports.match = match = function(string, query, options) {
    var allowErrors, baseMatches, matches, pathSeparator, preparedQuery, string_lw;
    allowErrors = options.allowErrors, preparedQuery = options.preparedQuery, pathSeparator = options.pathSeparator;
    if (!(allowErrors || isMatch(string, preparedQuery.core_lw, preparedQuery.core_up))) {
      return [];
    }
    string_lw = string.toLowerCase();
    matches = computeMatch(string, string_lw, preparedQuery);
    if (matches.length === 0) {
      return matches;
    }
    if (string.indexOf(pathSeparator) > -1) {
      baseMatches = basenameMatch(string, string_lw, preparedQuery, pathSeparator);
      matches = mergeMatches(matches, baseMatches);
    }
    return matches;
  };

  exports.wrap = function(string, query, options) {
    var matchIndex, matchPos, matchPositions, output, strPos, tagClass, tagClose, tagOpen, _ref1;
    if ((options.wrap != null)) {
      _ref1 = options.wrap, tagClass = _ref1.tagClass, tagOpen = _ref1.tagOpen, tagClose = _ref1.tagClose;
    }
    if (tagClass == null) {
      tagClass = 'highlight';
    }
    if (tagOpen == null) {
      tagOpen = '<strong class="' + tagClass + '">';
    }
    if (tagClose == null) {
      tagClose = '</strong>';
    }
    if (string === query) {
      return tagOpen + string + tagClose;
    }
    matchPositions = match(string, query, options);
    if (matchPositions.length === 0) {
      return string;
    }
    output = '';
    matchIndex = -1;
    strPos = 0;
    while (++matchIndex < matchPositions.length) {
      matchPos = matchPositions[matchIndex];
      if (matchPos > strPos) {
        output += string.substring(strPos, matchPos);
        strPos = matchPos;
      }
      while (++matchIndex < matchPositions.length) {
        if (matchPositions[matchIndex] === matchPos + 1) {
          matchPos++;
        } else {
          matchIndex--;
          break;
        }
      }
      matchPos++;
      if (matchPos > strPos) {
        output += tagOpen;
        output += string.substring(strPos, matchPos);
        output += tagClose;
        strPos = matchPos;
      }
    }
    if (strPos <= string.length - 1) {
      output += string.substring(strPos);
    }
    return output;
  };

  basenameMatch = function(subject, subject_lw, preparedQuery, pathSeparator) {
    var basePos, depth, end;
    end = subject.length - 1;
    while (subject[end] === pathSeparator) {
      end--;
    }
    basePos = subject.lastIndexOf(pathSeparator, end);
    if (basePos === -1) {
      return [];
    }
    depth = preparedQuery.depth;
    while (depth-- > 0) {
      basePos = subject.lastIndexOf(pathSeparator, basePos - 1);
      if (basePos === -1) {
        return [];
      }
    }
    basePos++;
    end++;
    return computeMatch(subject.slice(basePos, end), subject_lw.slice(basePos, end), preparedQuery, basePos);
  };

  mergeMatches = function(a, b) {
    var ai, bj, i, j, m, n, out;
    m = a.length;
    n = b.length;
    if (n === 0) {
      return a.slice();
    }
    if (m === 0) {
      return b.slice();
    }
    i = -1;
    j = 0;
    bj = b[j];
    out = [];
    while (++i < m) {
      ai = a[i];
      while (bj <= ai && ++j < n) {
        if (bj < ai) {
          out.push(bj);
        }
        bj = b[j];
      }
      out.push(ai);
    }
    while (j < n) {
      out.push(b[j++]);
    }
    return out;
  };

  computeMatch = function(subject, subject_lw, preparedQuery, offset) {
    var DIAGONAL, LEFT, STOP, UP, acro_score, align, backtrack, csc_diag, csc_row, csc_score, i, j, m, matches, move, n, pos, query, query_lw, score, score_diag, score_row, score_up, si_lw, start, trace;
    if (offset == null) {
      offset = 0;
    }
    query = preparedQuery.query;
    query_lw = preparedQuery.query_lw;
    m = subject.length;
    n = query.length;
    acro_score = scoreAcronyms(subject, subject_lw, query, query_lw).score;
    score_row = new Array(n);
    csc_row = new Array(n);
    STOP = 0;
    UP = 1;
    LEFT = 2;
    DIAGONAL = 3;
    trace = new Array(m * n);
    pos = -1;
    j = -1;
    while (++j < n) {
      score_row[j] = 0;
      csc_row[j] = 0;
    }
    i = -1;
    while (++i < m) {
      score = 0;
      score_up = 0;
      csc_diag = 0;
      si_lw = subject_lw[i];
      j = -1;
      while (++j < n) {
        csc_score = 0;
        align = 0;
        score_diag = score_up;
        if (query_lw[j] === si_lw) {
          start = isWordStart(i, subject, subject_lw);
          csc_score = csc_diag > 0 ? csc_diag : scoreConsecutives(subject, subject_lw, query, query_lw, i, j, start);
          align = score_diag + scoreCharacter(i, j, start, acro_score, csc_score);
        }
        score_up = score_row[j];
        csc_diag = csc_row[j];
        if (score > score_up) {
          move = LEFT;
        } else {
          score = score_up;
          move = UP;
        }
        if (align > score) {
          score = align;
          move = DIAGONAL;
        } else {
          csc_score = 0;
        }
        score_row[j] = score;
        csc_row[j] = csc_score;
        trace[++pos] = score > 0 ? move : STOP;
      }
    }
    i = m - 1;
    j = n - 1;
    pos = i * n + j;
    backtrack = true;
    matches = [];
    while (backtrack && i >= 0 && j >= 0) {
      switch (trace[pos]) {
        case UP:
          i--;
          pos -= n;
          break;
        case LEFT:
          j--;
          pos--;
          break;
        case DIAGONAL:
          matches.push(i + offset);
          j--;
          i--;
          pos -= n + 1;
          break;
        default:
          backtrack = false;
      }
    }
    matches.reverse();
    return matches;
  };

}).call(this);


/***/ }),

/***/ 435:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.EXITING = exports.ENTERED = exports.ENTERING = exports.EXITED = exports.UNMOUNTED = undefined;

var _propTypes = __webpack_require__(0);

var PropTypes = _interopRequireWildcard(_propTypes);

var _react = __webpack_require__(6);

var _react2 = _interopRequireDefault(_react);

var _reactDom = __webpack_require__(112);

var _reactDom2 = _interopRequireDefault(_reactDom);

var _reactLifecyclesCompat = __webpack_require__(436);

var _PropTypes = __webpack_require__(437);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _objectWithoutProperties(obj, keys) { var target = {}; for (var i in obj) { if (keys.indexOf(i) >= 0) continue; if (!Object.prototype.hasOwnProperty.call(obj, i)) continue; target[i] = obj[i]; } return target; }

function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

function _possibleConstructorReturn(self, call) { if (!self) { throw new ReferenceError("this hasn't been initialised - super() hasn't been called"); } return call && (typeof call === "object" || typeof call === "function") ? call : self; }

function _inherits(subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass; }

var UNMOUNTED = exports.UNMOUNTED = 'unmounted';
var EXITED = exports.EXITED = 'exited';
var ENTERING = exports.ENTERING = 'entering';
var ENTERED = exports.ENTERED = 'entered';
var EXITING = exports.EXITING = 'exiting';

/**
 * The Transition component lets you describe a transition from one component
 * state to another _over time_ with a simple declarative API. Most commonly
 * it's used to animate the mounting and unmounting of a component, but can also
 * be used to describe in-place transition states as well.
 *
 * By default the `Transition` component does not alter the behavior of the
 * component it renders, it only tracks "enter" and "exit" states for the components.
 * It's up to you to give meaning and effect to those states. For example we can
 * add styles to a component when it enters or exits:
 *
 * ```jsx
 * import Transition from 'react-transition-group/Transition';
 *
 * const duration = 300;
 *
 * const defaultStyle = {
 *   transition: `opacity ${duration}ms ease-in-out`,
 *   opacity: 0,
 * }
 *
 * const transitionStyles = {
 *   entering: { opacity: 0 },
 *   entered:  { opacity: 1 },
 * };
 *
 * const Fade = ({ in: inProp }) => (
 *   <Transition in={inProp} timeout={duration}>
 *     {(state) => (
 *       <div style={{
 *         ...defaultStyle,
 *         ...transitionStyles[state]
 *       }}>
 *         I'm a fade Transition!
 *       </div>
 *     )}
 *   </Transition>
 * );
 * ```
 *
 * As noted the `Transition` component doesn't _do_ anything by itself to its child component.
 * What it does do is track transition states over time so you can update the
 * component (such as by adding styles or classes) when it changes states.
 *
 * There are 4 main states a Transition can be in:
 *  - `'entering'`
 *  - `'entered'`
 *  - `'exiting'`
 *  - `'exited'`
 *
 * Transition state is toggled via the `in` prop. When `true` the component begins the
 * "Enter" stage. During this stage, the component will shift from its current transition state,
 * to `'entering'` for the duration of the transition and then to the `'entered'` stage once
 * it's complete. Let's take the following example:
 *
 * ```jsx
 * state = { in: false };
 *
 * toggleEnterState = () => {
 *   this.setState({ in: true });
 * }
 *
 * render() {
 *   return (
 *     <div>
 *       <Transition in={this.state.in} timeout={500} />
 *       <button onClick={this.toggleEnterState}>Click to Enter</button>
 *     </div>
 *   );
 * }
 * ```
 *
 * When the button is clicked the component will shift to the `'entering'` state and
 * stay there for 500ms (the value of `timeout`) before it finally switches to `'entered'`.
 *
 * When `in` is `false` the same thing happens except the state moves from `'exiting'` to `'exited'`.
 *
 * ## Timing
 *
 * Timing is often the trickiest part of animation, mistakes can result in slight delays
 * that are hard to pin down. A common example is when you want to add an exit transition,
 * you should set the desired final styles when the state is `'exiting'`. That's when the
 * transition to those styles will start and, if you matched the `timeout` prop with the
 * CSS Transition duration, it will end exactly when the state changes to `'exited'`.
 *
 * > **Note**: For simpler transitions the `Transition` component might be enough, but
 * > take into account that it's platform-agnostic, while the `CSSTransition` component
 * > [forces reflows](https://github.com/reactjs/react-transition-group/blob/5007303e729a74be66a21c3e2205e4916821524b/src/CSSTransition.js#L208-L215)
 * > in order to make more complex transitions more predictable. For example, even though
 * > classes `example-enter` and `example-enter-active` are applied immediately one after
 * > another, you can still transition from one to the other because of the forced reflow
 * > (read [this issue](https://github.com/reactjs/react-transition-group/issues/159#issuecomment-322761171)
 * > for more info). Take this into account when choosing between `Transition` and
 * > `CSSTransition`.
 *
 * ## Example
 *
 * <iframe src="https://codesandbox.io/embed/741op4mmj0?fontsize=14" style="width:100%; height:500px; border:0; border-radius: 4px; overflow:hidden;" sandbox="allow-modals allow-forms allow-popups allow-scripts allow-same-origin"></iframe>
 *
 */

var Transition = function (_React$Component) {
  _inherits(Transition, _React$Component);

  function Transition(props, context) {
    _classCallCheck(this, Transition);

    var _this = _possibleConstructorReturn(this, _React$Component.call(this, props, context));

    var parentGroup = context.transitionGroup;
    // In the context of a TransitionGroup all enters are really appears
    var appear = parentGroup && !parentGroup.isMounting ? props.enter : props.appear;

    var initialStatus = void 0;

    _this.appearStatus = null;

    if (props.in) {
      if (appear) {
        initialStatus = EXITED;
        _this.appearStatus = ENTERING;
      } else {
        initialStatus = ENTERED;
      }
    } else {
      if (props.unmountOnExit || props.mountOnEnter) {
        initialStatus = UNMOUNTED;
      } else {
        initialStatus = EXITED;
      }
    }

    _this.state = { status: initialStatus };

    _this.nextCallback = null;
    return _this;
  }

  Transition.prototype.getChildContext = function getChildContext() {
    return { transitionGroup: null // allows for nested Transitions
    };
  };

  Transition.getDerivedStateFromProps = function getDerivedStateFromProps(_ref, prevState) {
    var nextIn = _ref.in;

    if (nextIn && prevState.status === UNMOUNTED) {
      return { status: EXITED };
    }
    return null;
  };

  // getSnapshotBeforeUpdate(prevProps) {
  //   let nextStatus = null

  //   if (prevProps !== this.props) {
  //     const { status } = this.state

  //     if (this.props.in) {
  //       if (status !== ENTERING && status !== ENTERED) {
  //         nextStatus = ENTERING
  //       }
  //     } else {
  //       if (status === ENTERING || status === ENTERED) {
  //         nextStatus = EXITING
  //       }
  //     }
  //   }

  //   return { nextStatus }
  // }

  Transition.prototype.componentDidMount = function componentDidMount() {
    this.updateStatus(true, this.appearStatus);
  };

  Transition.prototype.componentDidUpdate = function componentDidUpdate(prevProps) {
    var nextStatus = null;
    if (prevProps !== this.props) {
      var status = this.state.status;


      if (this.props.in) {
        if (status !== ENTERING && status !== ENTERED) {
          nextStatus = ENTERING;
        }
      } else {
        if (status === ENTERING || status === ENTERED) {
          nextStatus = EXITING;
        }
      }
    }
    this.updateStatus(false, nextStatus);
  };

  Transition.prototype.componentWillUnmount = function componentWillUnmount() {
    this.cancelNextCallback();
  };

  Transition.prototype.getTimeouts = function getTimeouts() {
    var timeout = this.props.timeout;

    var exit = void 0,
        enter = void 0,
        appear = void 0;

    exit = enter = appear = timeout;

    if (timeout != null && typeof timeout !== 'number') {
      exit = timeout.exit;
      enter = timeout.enter;
      appear = timeout.appear;
    }
    return { exit: exit, enter: enter, appear: appear };
  };

  Transition.prototype.updateStatus = function updateStatus() {
    var mounting = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : false;
    var nextStatus = arguments[1];

    if (nextStatus !== null) {
      // nextStatus will always be ENTERING or EXITING.
      this.cancelNextCallback();
      var node = _reactDom2.default.findDOMNode(this);

      if (nextStatus === ENTERING) {
        this.performEnter(node, mounting);
      } else {
        this.performExit(node);
      }
    } else if (this.props.unmountOnExit && this.state.status === EXITED) {
      this.setState({ status: UNMOUNTED });
    }
  };

  Transition.prototype.performEnter = function performEnter(node, mounting) {
    var _this2 = this;

    var enter = this.props.enter;

    var appearing = this.context.transitionGroup ? this.context.transitionGroup.isMounting : mounting;

    var timeouts = this.getTimeouts();

    // no enter animation skip right to ENTERED
    // if we are mounting and running this it means appear _must_ be set
    if (!mounting && !enter) {
      this.safeSetState({ status: ENTERED }, function () {
        _this2.props.onEntered(node);
      });
      return;
    }

    this.props.onEnter(node, appearing);

    this.safeSetState({ status: ENTERING }, function () {
      _this2.props.onEntering(node, appearing);

      // FIXME: appear timeout?
      _this2.onTransitionEnd(node, timeouts.enter, function () {
        _this2.safeSetState({ status: ENTERED }, function () {
          _this2.props.onEntered(node, appearing);
        });
      });
    });
  };

  Transition.prototype.performExit = function performExit(node) {
    var _this3 = this;

    var exit = this.props.exit;

    var timeouts = this.getTimeouts();

    // no exit animation skip right to EXITED
    if (!exit) {
      this.safeSetState({ status: EXITED }, function () {
        _this3.props.onExited(node);
      });
      return;
    }
    this.props.onExit(node);

    this.safeSetState({ status: EXITING }, function () {
      _this3.props.onExiting(node);

      _this3.onTransitionEnd(node, timeouts.exit, function () {
        _this3.safeSetState({ status: EXITED }, function () {
          _this3.props.onExited(node);
        });
      });
    });
  };

  Transition.prototype.cancelNextCallback = function cancelNextCallback() {
    if (this.nextCallback !== null) {
      this.nextCallback.cancel();
      this.nextCallback = null;
    }
  };

  Transition.prototype.safeSetState = function safeSetState(nextState, callback) {
    // This shouldn't be necessary, but there are weird race conditions with
    // setState callbacks and unmounting in testing, so always make sure that
    // we can cancel any pending setState callbacks after we unmount.
    callback = this.setNextCallback(callback);
    this.setState(nextState, callback);
  };

  Transition.prototype.setNextCallback = function setNextCallback(callback) {
    var _this4 = this;

    var active = true;

    this.nextCallback = function (event) {
      if (active) {
        active = false;
        _this4.nextCallback = null;

        callback(event);
      }
    };

    this.nextCallback.cancel = function () {
      active = false;
    };

    return this.nextCallback;
  };

  Transition.prototype.onTransitionEnd = function onTransitionEnd(node, timeout, handler) {
    this.setNextCallback(handler);

    if (node) {
      if (this.props.addEndListener) {
        this.props.addEndListener(node, this.nextCallback);
      }
      if (timeout != null) {
        setTimeout(this.nextCallback, timeout);
      }
    } else {
      setTimeout(this.nextCallback, 0);
    }
  };

  Transition.prototype.render = function render() {
    var status = this.state.status;
    if (status === UNMOUNTED) {
      return null;
    }

    var _props = this.props,
        children = _props.children,
        childProps = _objectWithoutProperties(_props, ['children']);
    // filter props for Transtition


    delete childProps.in;
    delete childProps.mountOnEnter;
    delete childProps.unmountOnExit;
    delete childProps.appear;
    delete childProps.enter;
    delete childProps.exit;
    delete childProps.timeout;
    delete childProps.addEndListener;
    delete childProps.onEnter;
    delete childProps.onEntering;
    delete childProps.onEntered;
    delete childProps.onExit;
    delete childProps.onExiting;
    delete childProps.onExited;

    if (typeof children === 'function') {
      return children(status, childProps);
    }

    var child = _react2.default.Children.only(children);
    return _react2.default.cloneElement(child, childProps);
  };

  return Transition;
}(_react2.default.Component);

Transition.contextTypes = {
  transitionGroup: PropTypes.object
};
Transition.childContextTypes = {
  transitionGroup: function transitionGroup() {}
};


Transition.propTypes =  false ? {
  /**
   * A `function` child can be used instead of a React element.
   * This function is called with the current transition status
   * ('entering', 'entered', 'exiting', 'exited', 'unmounted'), which can be used
   * to apply context specific props to a component.
   *
   * ```jsx
   * <Transition timeout={150}>
   *   {(status) => (
   *     <MyComponent className={`fade fade-${status}`} />
   *   )}
   * </Transition>
   * ```
   */
  children: PropTypes.oneOfType([PropTypes.func.isRequired, PropTypes.element.isRequired]).isRequired,

  /**
   * Show the component; triggers the enter or exit states
   */
  in: PropTypes.bool,

  /**
   * By default the child component is mounted immediately along with
   * the parent `Transition` component. If you want to "lazy mount" the component on the
   * first `in={true}` you can set `mountOnEnter`. After the first enter transition the component will stay
   * mounted, even on "exited", unless you also specify `unmountOnExit`.
   */
  mountOnEnter: PropTypes.bool,

  /**
   * By default the child component stays mounted after it reaches the `'exited'` state.
   * Set `unmountOnExit` if you'd prefer to unmount the component after it finishes exiting.
   */
  unmountOnExit: PropTypes.bool,

  /**
   * Normally a component is not transitioned if it is shown when the `<Transition>` component mounts.
   * If you want to transition on the first mount set `appear` to `true`, and the
   * component will transition in as soon as the `<Transition>` mounts.
   *
   * > Note: there are no specific "appear" states. `appear` only adds an additional `enter` transition.
   */
  appear: PropTypes.bool,

  /**
   * Enable or disable enter transitions.
   */
  enter: PropTypes.bool,

  /**
   * Enable or disable exit transitions.
   */
  exit: PropTypes.bool,

  /**
   * The duration of the transition, in milliseconds.
   * Required unless `addEndListener` is provided
   *
   * You may specify a single timeout for all transitions like: `timeout={500}`,
   * or individually like:
   *
   * ```jsx
   * timeout={{
   *  enter: 300,
   *  exit: 500,
   * }}
   * ```
   *
   * @type {number | { enter?: number, exit?: number }}
   */
  timeout: function timeout(props) {
    for (var _len = arguments.length, args = Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
      args[_key - 1] = arguments[_key];
    }

    var pt = _PropTypes.timeoutsShape;
    if (!props.addEndListener) pt = pt.isRequired;
    return pt.apply(undefined, [props].concat(args));
  },

  /**
   * Add a custom transition end trigger. Called with the transitioning
   * DOM node and a `done` callback. Allows for more fine grained transition end
   * logic. **Note:** Timeouts are still used as a fallback if provided.
   *
   * ```jsx
   * addEndListener={(node, done) => {
   *   // use the css transitionend event to mark the finish of a transition
   *   node.addEventListener('transitionend', done, false);
   * }}
   * ```
   */
  addEndListener: PropTypes.func,

  /**
   * Callback fired before the "entering" status is applied. An extra parameter
   * `isAppearing` is supplied to indicate if the enter stage is occurring on the initial mount
   *
   * @type Function(node: HtmlElement, isAppearing: bool) -> void
   */
  onEnter: PropTypes.func,

  /**
   * Callback fired after the "entering" status is applied. An extra parameter
   * `isAppearing` is supplied to indicate if the enter stage is occurring on the initial mount
   *
   * @type Function(node: HtmlElement, isAppearing: bool)
   */
  onEntering: PropTypes.func,

  /**
   * Callback fired after the "entered" status is applied. An extra parameter
   * `isAppearing` is supplied to indicate if the enter stage is occurring on the initial mount
   *
   * @type Function(node: HtmlElement, isAppearing: bool) -> void
   */
  onEntered: PropTypes.func,

  /**
   * Callback fired before the "exiting" status is applied.
   *
   * @type Function(node: HtmlElement) -> void
   */
  onExit: PropTypes.func,

  /**
   * Callback fired after the "exiting" status is applied.
   *
   * @type Function(node: HtmlElement) -> void
   */
  onExiting: PropTypes.func,

  /**
   * Callback fired after the "exited" status is applied.
   *
   * @type Function(node: HtmlElement) -> void
   */
  onExited: PropTypes.func

  // Name the function so it is clearer in the documentation
} : {};function noop() {}

Transition.defaultProps = {
  in: false,
  mountOnEnter: false,
  unmountOnExit: false,
  appear: false,
  enter: true,
  exit: true,

  onEnter: noop,
  onEntering: noop,
  onEntered: noop,

  onExit: noop,
  onExiting: noop,
  onExited: noop
};

Transition.UNMOUNTED = 0;
Transition.EXITED = 1;
Transition.ENTERING = 2;
Transition.ENTERED = 3;
Transition.EXITING = 4;

exports.default = (0, _reactLifecyclesCompat.polyfill)(Transition);

/***/ }),

/***/ 436:
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
Object.defineProperty(__webpack_exports__, "__esModule", { value: true });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "polyfill", function() { return polyfill; });
/**
 * Copyright (c) 2013-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

function componentWillMount() {
  // Call this.constructor.gDSFP to support sub-classes.
  var state = this.constructor.getDerivedStateFromProps(this.props, this.state);
  if (state !== null && state !== undefined) {
    this.setState(state);
  }
}

function componentWillReceiveProps(nextProps) {
  // Call this.constructor.gDSFP to support sub-classes.
  // Use the setState() updater to ensure state isn't stale in certain edge cases.
  function updater(prevState) {
    var state = this.constructor.getDerivedStateFromProps(nextProps, prevState);
    return state !== null && state !== undefined ? state : null;
  }
  // Binding "this" is important for shallow renderer support.
  this.setState(updater.bind(this));
}

function componentWillUpdate(nextProps, nextState) {
  try {
    var prevProps = this.props;
    var prevState = this.state;
    this.props = nextProps;
    this.state = nextState;
    this.__reactInternalSnapshotFlag = true;
    this.__reactInternalSnapshot = this.getSnapshotBeforeUpdate(
      prevProps,
      prevState
    );
  } finally {
    this.props = prevProps;
    this.state = prevState;
  }
}

// React may warn about cWM/cWRP/cWU methods being deprecated.
// Add a flag to suppress these warnings for this special case.
componentWillMount.__suppressDeprecationWarning = true;
componentWillReceiveProps.__suppressDeprecationWarning = true;
componentWillUpdate.__suppressDeprecationWarning = true;

function polyfill(Component) {
  var prototype = Component.prototype;

  if (!prototype || !prototype.isReactComponent) {
    throw new Error('Can only polyfill class components');
  }

  if (
    typeof Component.getDerivedStateFromProps !== 'function' &&
    typeof prototype.getSnapshotBeforeUpdate !== 'function'
  ) {
    return Component;
  }

  // If new component APIs are defined, "unsafe" lifecycles won't be called.
  // Error if any of these lifecycles are present,
  // Because they would work differently between older and newer (16.3+) versions of React.
  var foundWillMountName = null;
  var foundWillReceivePropsName = null;
  var foundWillUpdateName = null;
  if (typeof prototype.componentWillMount === 'function') {
    foundWillMountName = 'componentWillMount';
  } else if (typeof prototype.UNSAFE_componentWillMount === 'function') {
    foundWillMountName = 'UNSAFE_componentWillMount';
  }
  if (typeof prototype.componentWillReceiveProps === 'function') {
    foundWillReceivePropsName = 'componentWillReceiveProps';
  } else if (typeof prototype.UNSAFE_componentWillReceiveProps === 'function') {
    foundWillReceivePropsName = 'UNSAFE_componentWillReceiveProps';
  }
  if (typeof prototype.componentWillUpdate === 'function') {
    foundWillUpdateName = 'componentWillUpdate';
  } else if (typeof prototype.UNSAFE_componentWillUpdate === 'function') {
    foundWillUpdateName = 'UNSAFE_componentWillUpdate';
  }
  if (
    foundWillMountName !== null ||
    foundWillReceivePropsName !== null ||
    foundWillUpdateName !== null
  ) {
    var componentName = Component.displayName || Component.name;
    var newApiName =
      typeof Component.getDerivedStateFromProps === 'function'
        ? 'getDerivedStateFromProps()'
        : 'getSnapshotBeforeUpdate()';

    throw Error(
      'Unsafe legacy lifecycles will not be called for components using new component APIs.\n\n' +
        componentName +
        ' uses ' +
        newApiName +
        ' but also contains the following legacy lifecycles:' +
        (foundWillMountName !== null ? '\n  ' + foundWillMountName : '') +
        (foundWillReceivePropsName !== null
          ? '\n  ' + foundWillReceivePropsName
          : '') +
        (foundWillUpdateName !== null ? '\n  ' + foundWillUpdateName : '') +
        '\n\nThe above lifecycles should be removed. Learn more about this warning here:\n' +
        'https://fb.me/react-async-component-lifecycle-hooks'
    );
  }

  // React <= 16.2 does not support static getDerivedStateFromProps.
  // As a workaround, use cWM and cWRP to invoke the new static lifecycle.
  // Newer versions of React will ignore these lifecycles if gDSFP exists.
  if (typeof Component.getDerivedStateFromProps === 'function') {
    prototype.componentWillMount = componentWillMount;
    prototype.componentWillReceiveProps = componentWillReceiveProps;
  }

  // React <= 16.2 does not support getSnapshotBeforeUpdate.
  // As a workaround, use cWU to invoke the new lifecycle.
  // Newer versions of React will ignore that lifecycle if gSBU exists.
  if (typeof prototype.getSnapshotBeforeUpdate === 'function') {
    if (typeof prototype.componentDidUpdate !== 'function') {
      throw new Error(
        'Cannot polyfill getSnapshotBeforeUpdate() for components that do not define componentDidUpdate() on the prototype'
      );
    }

    prototype.componentWillUpdate = componentWillUpdate;

    var componentDidUpdate = prototype.componentDidUpdate;

    prototype.componentDidUpdate = function componentDidUpdatePolyfill(
      prevProps,
      prevState,
      maybeSnapshot
    ) {
      // 16.3+ will not execute our will-update method;
      // It will pass a snapshot value to did-update though.
      // Older versions will require our polyfilled will-update value.
      // We need to handle both cases, but can't just check for the presence of "maybeSnapshot",
      // Because for <= 15.x versions this might be a "prevContext" object.
      // We also can't just check "__reactInternalSnapshot",
      // Because get-snapshot might return a falsy value.
      // So check for the explicit __reactInternalSnapshotFlag flag to determine behavior.
      var snapshot = this.__reactInternalSnapshotFlag
        ? this.__reactInternalSnapshot
        : maybeSnapshot;

      componentDidUpdate.call(this, prevProps, prevState, snapshot);
    };
  }

  return Component;
}




/***/ }),

/***/ 437:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.classNamesShape = exports.timeoutsShape = undefined;
exports.transitionTimeout = transitionTimeout;

var _propTypes = __webpack_require__(0);

var _propTypes2 = _interopRequireDefault(_propTypes);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function transitionTimeout(transitionType) {
  var timeoutPropName = 'transition' + transitionType + 'Timeout';
  var enabledPropName = 'transition' + transitionType;

  return function (props) {
    // If the transition is enabled
    if (props[enabledPropName]) {
      // If no timeout duration is provided
      if (props[timeoutPropName] == null) {
        return new Error(timeoutPropName + ' wasn\'t supplied to CSSTransitionGroup: ' + 'this can cause unreliable animations and won\'t be supported in ' + 'a future version of React. See ' + 'https://fb.me/react-animation-transition-group-timeout for more ' + 'information.');

        // If the duration isn't a number
      } else if (typeof props[timeoutPropName] !== 'number') {
        return new Error(timeoutPropName + ' must be a number (in milliseconds)');
      }
    }

    return null;
  };
}

var timeoutsShape = exports.timeoutsShape = _propTypes2.default.oneOfType([_propTypes2.default.number, _propTypes2.default.shape({
  enter: _propTypes2.default.number,
  exit: _propTypes2.default.number
}).isRequired]);

var classNamesShape = exports.classNamesShape = _propTypes2.default.oneOfType([_propTypes2.default.string, _propTypes2.default.shape({
  enter: _propTypes2.default.string,
  exit: _propTypes2.default.string,
  active: _propTypes2.default.string
}), _propTypes2.default.shape({
  enter: _propTypes2.default.string,
  enterDone: _propTypes2.default.string,
  enterActive: _propTypes2.default.string,
  exit: _propTypes2.default.string,
  exitDone: _propTypes2.default.string,
  exitActive: _propTypes2.default.string
})]);

/***/ }),

/***/ 438:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
Object.defineProperty(exports, "TabList", {
  enumerable: true,
  get: function () {
    return _tabList.default;
  }
});
Object.defineProperty(exports, "TabPanels", {
  enumerable: true,
  get: function () {
    return _tabPanels.default;
  }
});
Object.defineProperty(exports, "Tab", {
  enumerable: true,
  get: function () {
    return _tab.default;
  }
});
Object.defineProperty(exports, "Tabs", {
  enumerable: true,
  get: function () {
    return _tabs.default;
  }
});

var _tabList = _interopRequireDefault(__webpack_require__(185));

var _tabPanels = _interopRequireDefault(__webpack_require__(187));

var _tab = _interopRequireDefault(__webpack_require__(186));

var _tabs = _interopRequireDefault(__webpack_require__(442));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/***/ }),

/***/ 439:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _propTypes = _interopRequireDefault(__webpack_require__(0));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var _default = _propTypes.default.object;
exports.default = _default;

/***/ }),

/***/ 440:
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),

/***/ 441:
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),

/***/ 442:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _propTypes = _interopRequireDefault(__webpack_require__(0));

var _react = _interopRequireDefault(__webpack_require__(6));

var _uniqueId = _interopRequireDefault(__webpack_require__(443));

var _tabList = _interopRequireDefault(__webpack_require__(185));

var _tabPanels = _interopRequireDefault(__webpack_require__(187));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

class Tabs extends _react.default.Component {
  constructor() {
    super();
    this.accessibleId = (0, _uniqueId.default)();
  }

  render() {
    const {
      activeIndex,
      children,
      className,
      onActivateTab
    } = this.props;
    const accessibleId = this.accessibleId;
    return _react.default.createElement("div", {
      className: className
    }, _react.default.Children.map(children, child => {
      if (!child) {
        return child;
      }

      switch (child.type) {
        case _tabList.default:
          return _react.default.cloneElement(child, {
            accessibleId,
            activeIndex,
            onActivateTab
          });

        case _tabPanels.default:
          return _react.default.cloneElement(child, {
            accessibleId,
            activeIndex
          });

        default:
          return child;
      }
    }));
  }

}

exports.default = Tabs;
Tabs.propTypes = {
  activeIndex: _propTypes.default.number.isRequired,
  children: _propTypes.default.node,
  className: _propTypes.default.string,
  onActivateTab: _propTypes.default.func
};
Tabs.defaultProps = {
  children: null,
  className: undefined,
  onActivateTab: () => {}
};

/***/ }),

/***/ 443:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = uniqueId;
let counter = 0;

function uniqueId() {
  counter += 1;
  return `$rac$${counter}`;
}

/***/ }),

/***/ 445:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const SplitBox = __webpack_require__(446);

module.exports = SplitBox;

/***/ }),

/***/ 446:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const React = __webpack_require__(6);

const ReactDOM = __webpack_require__(112);

const Draggable = React.createFactory(__webpack_require__(447));
const {
  Component
} = React;

const PropTypes = __webpack_require__(0);

const dom = __webpack_require__(1);

__webpack_require__(448);
/**
 * This component represents a Splitter. The splitter supports vertical
 * as well as horizontal mode.
 */


class SplitBox extends Component {
  static get propTypes() {
    return {
      // Custom class name. You can use more names separated by a space.
      className: PropTypes.string,
      // Initial size of controlled panel.
      initialSize: PropTypes.any,
      // Optional initial width of controlled panel.
      initialWidth: PropTypes.number,
      // Optional initial height of controlled panel.
      initialHeight: PropTypes.number,
      // Left/top panel
      startPanel: PropTypes.any,
      // Left/top panel collapse state.
      startPanelCollapsed: PropTypes.bool,
      // Min panel size.
      minSize: PropTypes.any,
      // Max panel size.
      maxSize: PropTypes.any,
      // Right/bottom panel
      endPanel: PropTypes.any,
      // Right/bottom panel collapse state.
      endPanelCollapsed: PropTypes.bool,
      // True if the right/bottom panel should be controlled.
      endPanelControl: PropTypes.bool,
      // Size of the splitter handle bar.
      splitterSize: PropTypes.number,
      // True if the splitter bar is vertical (default is vertical).
      vert: PropTypes.bool,
      // Optional style properties passed into the splitbox
      style: PropTypes.object,
      // Optional callback when splitbox resize stops
      onResizeEnd: PropTypes.func
    };
  }

  static get defaultProps() {
    return {
      splitterSize: 5,
      vert: true,
      endPanelControl: false,
      endPanelCollapsed: false,
      startPanelCollapsed: false
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      vert: props.vert,
      // We use integers for these properties
      width: parseInt(props.initialWidth || props.initialSize, 10),
      height: parseInt(props.initialHeight || props.initialSize, 10)
    };
    this.onStartMove = this.onStartMove.bind(this);
    this.onStopMove = this.onStopMove.bind(this);
    this.onMove = this.onMove.bind(this);
    this.preparePanelStyles = this.preparePanelStyles.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    if (this.props.vert !== nextProps.vert) {
      this.setState({
        vert: nextProps.vert
      });
    }

    if (this.props.initialSize !== nextProps.initialSize || this.props.initialWidth !== nextProps.initialWidth || this.props.initialHeight !== nextProps.initialHeight) {
      this.setState({
        width: parseInt(nextProps.initialWidth || nextProps.initialSize, 10),
        height: parseInt(nextProps.initialHeight || nextProps.initialSize, 10)
      });
    }
  } // Dragging Events

  /**
   * Set 'resizing' cursor on entire document during splitter dragging.
   * This avoids cursor-flickering that happens when the mouse leaves
   * the splitter bar area (happens frequently).
   */


  onStartMove() {
    const splitBox = ReactDOM.findDOMNode(this);
    const doc = splitBox.ownerDocument;
    const defaultCursor = doc.documentElement.style.cursor;
    doc.documentElement.style.cursor = this.state.vert ? "ew-resize" : "ns-resize";
    splitBox.classList.add("dragging");
    document.dispatchEvent(new CustomEvent("drag:start"));
    this.setState({
      defaultCursor: defaultCursor
    });
  }

  onStopMove() {
    const splitBox = ReactDOM.findDOMNode(this);
    const doc = splitBox.ownerDocument;
    doc.documentElement.style.cursor = this.state.defaultCursor;
    splitBox.classList.remove("dragging");
    document.dispatchEvent(new CustomEvent("drag:end"));

    if (this.props.onResizeEnd) {
      this.props.onResizeEnd(this.state.vert ? this.state.width : this.state.height);
    }
  }
  /**
   * Adjust size of the controlled panel. Depending on the current
   * orientation we either remember the width or height of
   * the splitter box.
   */


  onMove({
    movementX,
    movementY
  }) {
    const node = ReactDOM.findDOMNode(this);
    const doc = node.ownerDocument;

    if (this.props.endPanelControl) {
      // For the end panel we need to increase the width/height when the
      // movement is towards the left/top.
      movementX = -movementX;
      movementY = -movementY;
    }

    if (this.state.vert) {
      const isRtl = doc.dir === "rtl";

      if (isRtl) {
        // In RTL we need to reverse the movement again -- but only for vertical
        // splitters
        movementX = -movementX;
      }

      this.setState((state, props) => ({
        width: state.width + movementX
      }));
    } else {
      this.setState((state, props) => ({
        height: state.height + movementY
      }));
    }
  } // Rendering


  preparePanelStyles() {
    const vert = this.state.vert;
    const {
      minSize,
      maxSize,
      startPanelCollapsed,
      endPanelControl,
      endPanelCollapsed
    } = this.props;
    let leftPanelStyle, rightPanelStyle; // Set proper size for panels depending on the current state.

    if (vert) {
      const startWidth = endPanelControl ? null : this.state.width,
            endWidth = endPanelControl ? this.state.width : null;
      leftPanelStyle = {
        maxWidth: endPanelControl ? null : maxSize,
        minWidth: endPanelControl ? null : minSize,
        width: startPanelCollapsed ? 0 : startWidth
      };
      rightPanelStyle = {
        maxWidth: endPanelControl ? maxSize : null,
        minWidth: endPanelControl ? minSize : null,
        width: endPanelCollapsed ? 0 : endWidth
      };
    } else {
      const startHeight = endPanelControl ? null : this.state.height,
            endHeight = endPanelControl ? this.state.height : null;
      leftPanelStyle = {
        maxHeight: endPanelControl ? null : maxSize,
        minHeight: endPanelControl ? null : minSize,
        height: endPanelCollapsed ? maxSize : startHeight
      };
      rightPanelStyle = {
        maxHeight: endPanelControl ? maxSize : null,
        minHeight: endPanelControl ? minSize : null,
        height: startPanelCollapsed ? maxSize : endHeight
      };
    }

    return {
      leftPanelStyle,
      rightPanelStyle
    };
  }

  render() {
    const vert = this.state.vert;
    const {
      startPanelCollapsed,
      startPanel,
      endPanel,
      endPanelControl,
      splitterSize,
      endPanelCollapsed
    } = this.props;
    const style = Object.assign({}, this.props.style); // Calculate class names list.

    let classNames = ["split-box"];
    classNames.push(vert ? "vert" : "horz");

    if (this.props.className) {
      classNames = classNames.concat(this.props.className.split(" "));
    }

    const {
      leftPanelStyle,
      rightPanelStyle
    } = this.preparePanelStyles(); // Calculate splitter size

    const splitterStyle = {
      flex: `0 0 ${splitterSize}px`
    };
    return dom.div({
      className: classNames.join(" "),
      style: style
    }, !startPanelCollapsed ? dom.div({
      className: endPanelControl ? "uncontrolled" : "controlled",
      style: leftPanelStyle
    }, startPanel) : null, Draggable({
      className: "splitter",
      style: splitterStyle,
      onStart: this.onStartMove,
      onStop: this.onStopMove,
      onMove: this.onMove
    }), !endPanelCollapsed ? dom.div({
      className: endPanelControl ? "controlled" : "uncontrolled",
      style: rightPanelStyle
    }, endPanel) : null);
  }

}

module.exports = SplitBox;

/***/ }),

/***/ 447:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const React = __webpack_require__(6);

const ReactDOM = __webpack_require__(112);

const {
  Component
} = React;

const PropTypes = __webpack_require__(0);

const dom = __webpack_require__(1);

class Draggable extends Component {
  static get propTypes() {
    return {
      onMove: PropTypes.func.isRequired,
      onStart: PropTypes.func,
      onStop: PropTypes.func,
      style: PropTypes.object,
      className: PropTypes.string
    };
  }

  constructor(props) {
    super(props);
    this.startDragging = this.startDragging.bind(this);
    this.onMove = this.onMove.bind(this);
    this.onUp = this.onUp.bind(this);
  }

  startDragging(ev) {
    ev.preventDefault();
    const doc = ReactDOM.findDOMNode(this).ownerDocument;
    doc.addEventListener("mousemove", this.onMove);
    doc.addEventListener("mouseup", this.onUp);
    this.props.onStart && this.props.onStart();
  }

  onMove(ev) {
    ev.preventDefault(); // When the target is outside of the document, its tagName is undefined

    if (!ev.target.tagName) {
      return;
    } // We pass the whole event because we don't know which properties
    // the callee needs.


    this.props.onMove(ev);
  }

  onUp(ev) {
    ev.preventDefault();
    const doc = ReactDOM.findDOMNode(this).ownerDocument;
    doc.removeEventListener("mousemove", this.onMove);
    doc.removeEventListener("mouseup", this.onUp);
    this.props.onStop && this.props.onStop();
  }

  render() {
    return dom.div({
      style: this.props.style,
      className: this.props.className,
      onMouseDown: this.startDragging
    });
  }

}

module.exports = Draggable;

/***/ }),

/***/ 448:
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),

/***/ 449:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = move;

function _toConsumableArray(arr) { if (Array.isArray(arr)) { for (var i = 0, arr2 = Array(arr.length); i < arr.length; i++) { arr2[i] = arr[i]; } return arr2; } else { return Array.from(arr); } }

function move(array, moveIndex, toIndex) {
  /* #move - Moves an array item from one position in an array to another.
      Note: This is a pure function so a new array will be returned, instead
     of altering the array argument.
     Arguments:
    1. array     (String) : Array in which to move an item.         (required)
    2. moveIndex (Object) : The index of the item to move.          (required)
    3. toIndex   (Object) : The index to move item at moveIndex to. (required)
  */
  var item = array[moveIndex];
  var length = array.length;
  var diff = moveIndex - toIndex;

  if (diff > 0) {
    // move left
    return [].concat(_toConsumableArray(array.slice(0, toIndex)), [item], _toConsumableArray(array.slice(toIndex, moveIndex)), _toConsumableArray(array.slice(moveIndex + 1, length)));
  } else if (diff < 0) {
    // move right
    return [].concat(_toConsumableArray(array.slice(0, moveIndex)), _toConsumableArray(array.slice(moveIndex + 1, toIndex + 1)), [item], _toConsumableArray(array.slice(toIndex + 1, length)));
  }
  return array;
}

/***/ }),

/***/ 490:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_490__;

/***/ }),

/***/ 491:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_491__;

/***/ }),

/***/ 501:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// These are the available plural functions that give the appropriate index
// based on the plural rule number specified. The first element is the number
// of plural forms and the second is the function to figure out the index.
const gFunctions = [// 0: Chinese
[1, n => 0], // 1: English
[2, n => n != 1 ? 1 : 0], // 2: French
[2, n => n > 1 ? 1 : 0], // 3: Latvian
[3, n => n % 10 == 1 && n % 100 != 11 ? 1 : n % 10 == 0 ? 0 : 2], // 4: Scottish Gaelic
[4, n => n == 1 || n == 11 ? 0 : n == 2 || n == 12 ? 1 : n > 0 && n < 20 ? 2 : 3], // 5: Romanian
[3, n => n == 1 ? 0 : n == 0 || n % 100 > 0 && n % 100 < 20 ? 1 : 2], // 6: Lithuanian
[3, n => n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && (n % 100 < 10 || n % 100 >= 20) ? 2 : 1], // 7: Russian
[3, n => n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2], // 8: Slovak
[3, n => n == 1 ? 0 : n >= 2 && n <= 4 ? 1 : 2], // 9: Polish
[3, n => n == 1 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2], // 10: Slovenian
[4, n => n % 100 == 1 ? 0 : n % 100 == 2 ? 1 : n % 100 == 3 || n % 100 == 4 ? 2 : 3], // 11: Irish Gaeilge
[5, n => n == 1 ? 0 : n == 2 ? 1 : n >= 3 && n <= 6 ? 2 : n >= 7 && n <= 10 ? 3 : 4], // 12: Arabic
[6, n => n == 0 ? 5 : n == 1 ? 0 : n == 2 ? 1 : n % 100 >= 3 && n % 100 <= 10 ? 2 : n % 100 >= 11 && n % 100 <= 99 ? 3 : 4], // 13: Maltese
[4, n => n == 1 ? 0 : n == 0 || n % 100 > 0 && n % 100 <= 10 ? 1 : n % 100 > 10 && n % 100 < 20 ? 2 : 3], // 14: Unused
[3, n => n % 10 == 1 ? 0 : n % 10 == 2 ? 1 : 2], // 15: Icelandic, Macedonian
[2, n => n % 10 == 1 && n % 100 != 11 ? 0 : 1], // 16: Breton
[5, n => n % 10 == 1 && n % 100 != 11 && n % 100 != 71 && n % 100 != 91 ? 0 : n % 10 == 2 && n % 100 != 12 && n % 100 != 72 && n % 100 != 92 ? 1 : (n % 10 == 3 || n % 10 == 4 || n % 10 == 9) && n % 100 != 13 && n % 100 != 14 && n % 100 != 19 && n % 100 != 73 && n % 100 != 74 && n % 100 != 79 && n % 100 != 93 && n % 100 != 94 && n % 100 != 99 ? 2 : n % 1000000 == 0 && n != 0 ? 3 : 4], // 17: Shuar
[2, n => n != 0 ? 1 : 0], // 18: Welsh
[6, n => n == 0 ? 0 : n == 1 ? 1 : n == 2 ? 2 : n == 3 ? 3 : n == 6 ? 4 : 5], // 19: Bosnian, Croatian, Serbian
[3, n => n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2]];
const PluralForm = {
  /**
   * Get the correct plural form of a word based on the number
   *
   * @param aNum
   *        The number to decide which plural form to use
   * @param aWords
   *        A semi-colon (;) separated string of words to pick the plural form
   * @return The appropriate plural form of the word
   */
  get get() {
    // This method will lazily load to avoid perf when it is first needed and
    // creates getPluralForm function. The function it creates is based on the
    // value of pluralRule specified in the intl stringbundle.
    // See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
    // Delete the getters to be overwritten
    delete this.numForms;
    delete this.get; // Make the plural form get function and set it as the default get

    [this.get, this.numForms] = this.makeGetter(this.ruleNum);
    return this.get;
  },

  /**
   * Create a pair of plural form functions for the given plural rule number.
   *
   * @param aRuleNum
   *        The plural rule number to create functions
   * @return A pair: [function that gets the right plural form,
   *                  function that returns the number of plural forms]
   */
  makeGetter: function (aRuleNum) {
    // Default to "all plural" if the value is out of bounds or invalid
    if (aRuleNum < 0 || aRuleNum >= gFunctions.length || isNaN(aRuleNum)) {
      log(["Invalid rule number: ", aRuleNum, " -- defaulting to 0"]);
      aRuleNum = 0;
    } // Get the desired pluralRule function


    let [numForms, pluralFunc] = gFunctions[aRuleNum]; // Return functions that give 1) the number of forms and 2) gets the right
    // plural form

    return [function (aNum, aWords) {
      // Figure out which index to use for the semi-colon separated words
      let index = pluralFunc(aNum ? Number(aNum) : 0);
      let words = aWords ? aWords.split(/;/) : [""]; // Explicitly check bounds to avoid strict warnings

      let ret = index < words.length ? words[index] : undefined; // Check for array out of bounds or empty strings

      if (ret == undefined || ret == "") {
        // Display a message in the error console
        log(["Index #", index, " of '", aWords, "' for value ", aNum, " is invalid -- plural rule #", aRuleNum, ";"]); // Default to the first entry (which might be empty, but not undefined)

        ret = words[0];
      }

      return ret;
    }, () => numForms];
  },

  /**
   * Get the number of forms for the current plural rule
   *
   * @return The number of forms
   */
  get numForms() {
    // We lazily load numForms, so trigger the init logic with get()
    this.get();
    return this.numForms;
  },

  /**
   * Get the plural rule number from the intl stringbundle
   *
   * @return The plural rule number
   */
  get ruleNum() {
    try {
      return parseInt(L10N.getStr("pluralRule"), 10);
    } catch (e) {
      // Fallback to English if the pluralRule property is not available.
      return 1;
    }
  }

};
/**
 * Private helper function to log errors to the error console and command line
 *
 * @param aMsg
 *        Error message to log or an array of strings to concat
 */

function log(aMsg) {
  let msg = "plural-form.js: " + (aMsg.join ? aMsg.join("") : aMsg);
  console.log(msg + "\n");
}

module.exports = PluralForm;

/***/ }),

/***/ 516:
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */


const asyncStorage = __webpack_require__(427);
/*
 * asyncStoreHelper wraps asyncStorage so that it is easy to define project
 * specific properties. It is similar to PrefsHelper.
 *
 * e.g.
 *   const asyncStore = asyncStoreHelper("r", {a: "_a"})
 *   asyncStore.a         // => asyncStorage.getItem("r._a")
 *   asyncStore.a = 2     // => asyncStorage.setItem("r._a", 2)
 */


function asyncStoreHelper(root, mappings) {
  let store = {};

  function getMappingKey(key) {
    return Array.isArray(mappings[key]) ? mappings[key][0] : mappings[key];
  }

  function getMappingDefaultValue(key) {
    return Array.isArray(mappings[key]) ? mappings[key][1] : null;
  }

  Object.keys(mappings).map(key => Object.defineProperty(store, key, {
    async get() {
      const value = await asyncStorage.getItem(`${root}.${getMappingKey(key)}`);
      return value || getMappingDefaultValue(key);
    },

    set(value) {
      return asyncStorage.setItem(`${root}.${getMappingKey(key)}`, value);
    }

  }));
  store = new Proxy(store, {
    set: function (target, property, value, receiver) {
      if (!mappings.hasOwnProperty(property)) {
        throw new Error(`AsyncStore: ${property} is not defined in mappings`);
      }

      Reflect.set(...arguments);
      return true;
    }
  });
  return store;
}

module.exports = asyncStoreHelper;

/***/ }),

/***/ 520:
/***/ (function(module, exports) {

module.exports = () => {};

/***/ }),

/***/ 521:
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(module, global) {var __WEBPACK_AMD_DEFINE_RESULT__;/*! https://mths.be/punycode v1.4.1 by @mathias */
;(function(root) {

	/** Detect free variables */
	var freeExports = typeof exports == 'object' && exports &&
		!exports.nodeType && exports;
	var freeModule = typeof module == 'object' && module &&
		!module.nodeType && module;
	var freeGlobal = typeof global == 'object' && global;
	if (
		freeGlobal.global === freeGlobal ||
		freeGlobal.window === freeGlobal ||
		freeGlobal.self === freeGlobal
	) {
		root = freeGlobal;
	}

	/**
	 * The `punycode` object.
	 * @name punycode
	 * @type Object
	 */
	var punycode,

	/** Highest positive signed 32-bit float value */
	maxInt = 2147483647, // aka. 0x7FFFFFFF or 2^31-1

	/** Bootstring parameters */
	base = 36,
	tMin = 1,
	tMax = 26,
	skew = 38,
	damp = 700,
	initialBias = 72,
	initialN = 128, // 0x80
	delimiter = '-', // '\x2D'

	/** Regular expressions */
	regexPunycode = /^xn--/,
	regexNonASCII = /[^\x20-\x7E]/, // unprintable ASCII chars + non-ASCII chars
	regexSeparators = /[\x2E\u3002\uFF0E\uFF61]/g, // RFC 3490 separators

	/** Error messages */
	errors = {
		'overflow': 'Overflow: input needs wider integers to process',
		'not-basic': 'Illegal input >= 0x80 (not a basic code point)',
		'invalid-input': 'Invalid input'
	},

	/** Convenience shortcuts */
	baseMinusTMin = base - tMin,
	floor = Math.floor,
	stringFromCharCode = String.fromCharCode,

	/** Temporary variable */
	key;

	/*--------------------------------------------------------------------------*/

	/**
	 * A generic error utility function.
	 * @private
	 * @param {String} type The error type.
	 * @returns {Error} Throws a `RangeError` with the applicable error message.
	 */
	function error(type) {
		throw new RangeError(errors[type]);
	}

	/**
	 * A generic `Array#map` utility function.
	 * @private
	 * @param {Array} array The array to iterate over.
	 * @param {Function} callback The function that gets called for every array
	 * item.
	 * @returns {Array} A new array of values returned by the callback function.
	 */
	function map(array, fn) {
		var length = array.length;
		var result = [];
		while (length--) {
			result[length] = fn(array[length]);
		}
		return result;
	}

	/**
	 * A simple `Array#map`-like wrapper to work with domain name strings or email
	 * addresses.
	 * @private
	 * @param {String} domain The domain name or email address.
	 * @param {Function} callback The function that gets called for every
	 * character.
	 * @returns {Array} A new string of characters returned by the callback
	 * function.
	 */
	function mapDomain(string, fn) {
		var parts = string.split('@');
		var result = '';
		if (parts.length > 1) {
			// In email addresses, only the domain name should be punycoded. Leave
			// the local part (i.e. everything up to `@`) intact.
			result = parts[0] + '@';
			string = parts[1];
		}
		// Avoid `split(regex)` for IE8 compatibility. See #17.
		string = string.replace(regexSeparators, '\x2E');
		var labels = string.split('.');
		var encoded = map(labels, fn).join('.');
		return result + encoded;
	}

	/**
	 * Creates an array containing the numeric code points of each Unicode
	 * character in the string. While JavaScript uses UCS-2 internally,
	 * this function will convert a pair of surrogate halves (each of which
	 * UCS-2 exposes as separate characters) into a single code point,
	 * matching UTF-16.
	 * @see `punycode.ucs2.encode`
	 * @see <https://mathiasbynens.be/notes/javascript-encoding>
	 * @memberOf punycode.ucs2
	 * @name decode
	 * @param {String} string The Unicode input string (UCS-2).
	 * @returns {Array} The new array of code points.
	 */
	function ucs2decode(string) {
		var output = [],
		    counter = 0,
		    length = string.length,
		    value,
		    extra;
		while (counter < length) {
			value = string.charCodeAt(counter++);
			if (value >= 0xD800 && value <= 0xDBFF && counter < length) {
				// high surrogate, and there is a next character
				extra = string.charCodeAt(counter++);
				if ((extra & 0xFC00) == 0xDC00) { // low surrogate
					output.push(((value & 0x3FF) << 10) + (extra & 0x3FF) + 0x10000);
				} else {
					// unmatched surrogate; only append this code unit, in case the next
					// code unit is the high surrogate of a surrogate pair
					output.push(value);
					counter--;
				}
			} else {
				output.push(value);
			}
		}
		return output;
	}

	/**
	 * Creates a string based on an array of numeric code points.
	 * @see `punycode.ucs2.decode`
	 * @memberOf punycode.ucs2
	 * @name encode
	 * @param {Array} codePoints The array of numeric code points.
	 * @returns {String} The new Unicode string (UCS-2).
	 */
	function ucs2encode(array) {
		return map(array, function(value) {
			var output = '';
			if (value > 0xFFFF) {
				value -= 0x10000;
				output += stringFromCharCode(value >>> 10 & 0x3FF | 0xD800);
				value = 0xDC00 | value & 0x3FF;
			}
			output += stringFromCharCode(value);
			return output;
		}).join('');
	}

	/**
	 * Converts a basic code point into a digit/integer.
	 * @see `digitToBasic()`
	 * @private
	 * @param {Number} codePoint The basic numeric code point value.
	 * @returns {Number} The numeric value of a basic code point (for use in
	 * representing integers) in the range `0` to `base - 1`, or `base` if
	 * the code point does not represent a value.
	 */
	function basicToDigit(codePoint) {
		if (codePoint - 48 < 10) {
			return codePoint - 22;
		}
		if (codePoint - 65 < 26) {
			return codePoint - 65;
		}
		if (codePoint - 97 < 26) {
			return codePoint - 97;
		}
		return base;
	}

	/**
	 * Converts a digit/integer into a basic code point.
	 * @see `basicToDigit()`
	 * @private
	 * @param {Number} digit The numeric value of a basic code point.
	 * @returns {Number} The basic code point whose value (when used for
	 * representing integers) is `digit`, which needs to be in the range
	 * `0` to `base - 1`. If `flag` is non-zero, the uppercase form is
	 * used; else, the lowercase form is used. The behavior is undefined
	 * if `flag` is non-zero and `digit` has no uppercase form.
	 */
	function digitToBasic(digit, flag) {
		//  0..25 map to ASCII a..z or A..Z
		// 26..35 map to ASCII 0..9
		return digit + 22 + 75 * (digit < 26) - ((flag != 0) << 5);
	}

	/**
	 * Bias adaptation function as per section 3.4 of RFC 3492.
	 * https://tools.ietf.org/html/rfc3492#section-3.4
	 * @private
	 */
	function adapt(delta, numPoints, firstTime) {
		var k = 0;
		delta = firstTime ? floor(delta / damp) : delta >> 1;
		delta += floor(delta / numPoints);
		for (/* no initialization */; delta > baseMinusTMin * tMax >> 1; k += base) {
			delta = floor(delta / baseMinusTMin);
		}
		return floor(k + (baseMinusTMin + 1) * delta / (delta + skew));
	}

	/**
	 * Converts a Punycode string of ASCII-only symbols to a string of Unicode
	 * symbols.
	 * @memberOf punycode
	 * @param {String} input The Punycode string of ASCII-only symbols.
	 * @returns {String} The resulting string of Unicode symbols.
	 */
	function decode(input) {
		// Don't use UCS-2
		var output = [],
		    inputLength = input.length,
		    out,
		    i = 0,
		    n = initialN,
		    bias = initialBias,
		    basic,
		    j,
		    index,
		    oldi,
		    w,
		    k,
		    digit,
		    t,
		    /** Cached calculation results */
		    baseMinusT;

		// Handle the basic code points: let `basic` be the number of input code
		// points before the last delimiter, or `0` if there is none, then copy
		// the first basic code points to the output.

		basic = input.lastIndexOf(delimiter);
		if (basic < 0) {
			basic = 0;
		}

		for (j = 0; j < basic; ++j) {
			// if it's not a basic code point
			if (input.charCodeAt(j) >= 0x80) {
				error('not-basic');
			}
			output.push(input.charCodeAt(j));
		}

		// Main decoding loop: start just after the last delimiter if any basic code
		// points were copied; start at the beginning otherwise.

		for (index = basic > 0 ? basic + 1 : 0; index < inputLength; /* no final expression */) {

			// `index` is the index of the next character to be consumed.
			// Decode a generalized variable-length integer into `delta`,
			// which gets added to `i`. The overflow checking is easier
			// if we increase `i` as we go, then subtract off its starting
			// value at the end to obtain `delta`.
			for (oldi = i, w = 1, k = base; /* no condition */; k += base) {

				if (index >= inputLength) {
					error('invalid-input');
				}

				digit = basicToDigit(input.charCodeAt(index++));

				if (digit >= base || digit > floor((maxInt - i) / w)) {
					error('overflow');
				}

				i += digit * w;
				t = k <= bias ? tMin : (k >= bias + tMax ? tMax : k - bias);

				if (digit < t) {
					break;
				}

				baseMinusT = base - t;
				if (w > floor(maxInt / baseMinusT)) {
					error('overflow');
				}

				w *= baseMinusT;

			}

			out = output.length + 1;
			bias = adapt(i - oldi, out, oldi == 0);

			// `i` was supposed to wrap around from `out` to `0`,
			// incrementing `n` each time, so we'll fix that now:
			if (floor(i / out) > maxInt - n) {
				error('overflow');
			}

			n += floor(i / out);
			i %= out;

			// Insert `n` at position `i` of the output
			output.splice(i++, 0, n);

		}

		return ucs2encode(output);
	}

	/**
	 * Converts a string of Unicode symbols (e.g. a domain name label) to a
	 * Punycode string of ASCII-only symbols.
	 * @memberOf punycode
	 * @param {String} input The string of Unicode symbols.
	 * @returns {String} The resulting Punycode string of ASCII-only symbols.
	 */
	function encode(input) {
		var n,
		    delta,
		    handledCPCount,
		    basicLength,
		    bias,
		    j,
		    m,
		    q,
		    k,
		    t,
		    currentValue,
		    output = [],
		    /** `inputLength` will hold the number of code points in `input`. */
		    inputLength,
		    /** Cached calculation results */
		    handledCPCountPlusOne,
		    baseMinusT,
		    qMinusT;

		// Convert the input in UCS-2 to Unicode
		input = ucs2decode(input);

		// Cache the length
		inputLength = input.length;

		// Initialize the state
		n = initialN;
		delta = 0;
		bias = initialBias;

		// Handle the basic code points
		for (j = 0; j < inputLength; ++j) {
			currentValue = input[j];
			if (currentValue < 0x80) {
				output.push(stringFromCharCode(currentValue));
			}
		}

		handledCPCount = basicLength = output.length;

		// `handledCPCount` is the number of code points that have been handled;
		// `basicLength` is the number of basic code points.

		// Finish the basic string - if it is not empty - with a delimiter
		if (basicLength) {
			output.push(delimiter);
		}

		// Main encoding loop:
		while (handledCPCount < inputLength) {

			// All non-basic code points < n have been handled already. Find the next
			// larger one:
			for (m = maxInt, j = 0; j < inputLength; ++j) {
				currentValue = input[j];
				if (currentValue >= n && currentValue < m) {
					m = currentValue;
				}
			}

			// Increase `delta` enough to advance the decoder's <n,i> state to <m,0>,
			// but guard against overflow
			handledCPCountPlusOne = handledCPCount + 1;
			if (m - n > floor((maxInt - delta) / handledCPCountPlusOne)) {
				error('overflow');
			}

			delta += (m - n) * handledCPCountPlusOne;
			n = m;

			for (j = 0; j < inputLength; ++j) {
				currentValue = input[j];

				if (currentValue < n && ++delta > maxInt) {
					error('overflow');
				}

				if (currentValue == n) {
					// Represent delta as a generalized variable-length integer
					for (q = delta, k = base; /* no condition */; k += base) {
						t = k <= bias ? tMin : (k >= bias + tMax ? tMax : k - bias);
						if (q < t) {
							break;
						}
						qMinusT = q - t;
						baseMinusT = base - t;
						output.push(
							stringFromCharCode(digitToBasic(t + qMinusT % baseMinusT, 0))
						);
						q = floor(qMinusT / baseMinusT);
					}

					output.push(stringFromCharCode(digitToBasic(q, 0)));
					bias = adapt(delta, handledCPCountPlusOne, handledCPCount == basicLength);
					delta = 0;
					++handledCPCount;
				}
			}

			++delta;
			++n;

		}
		return output.join('');
	}

	/**
	 * Converts a Punycode string representing a domain name or an email address
	 * to Unicode. Only the Punycoded parts of the input will be converted, i.e.
	 * it doesn't matter if you call it on a string that has already been
	 * converted to Unicode.
	 * @memberOf punycode
	 * @param {String} input The Punycoded domain name or email address to
	 * convert to Unicode.
	 * @returns {String} The Unicode representation of the given Punycode
	 * string.
	 */
	function toUnicode(input) {
		return mapDomain(input, function(string) {
			return regexPunycode.test(string)
				? decode(string.slice(4).toLowerCase())
				: string;
		});
	}

	/**
	 * Converts a Unicode string representing a domain name or an email address to
	 * Punycode. Only the non-ASCII parts of the domain name will be converted,
	 * i.e. it doesn't matter if you call it with a domain that's already in
	 * ASCII.
	 * @memberOf punycode
	 * @param {String} input The domain name or email address to convert, as a
	 * Unicode string.
	 * @returns {String} The Punycode representation of the given domain name or
	 * email address.
	 */
	function toASCII(input) {
		return mapDomain(input, function(string) {
			return regexNonASCII.test(string)
				? 'xn--' + encode(string)
				: string;
		});
	}

	/*--------------------------------------------------------------------------*/

	/** Define the public API */
	punycode = {
		/**
		 * A string representing the current Punycode.js version number.
		 * @memberOf punycode
		 * @type String
		 */
		'version': '1.4.1',
		/**
		 * An object of methods to convert from JavaScript's internal character
		 * representation (UCS-2) to Unicode code points, and back.
		 * @see <https://mathiasbynens.be/notes/javascript-encoding>
		 * @memberOf punycode
		 * @type Object
		 */
		'ucs2': {
			'decode': ucs2decode,
			'encode': ucs2encode
		},
		'decode': decode,
		'encode': encode,
		'toASCII': toASCII,
		'toUnicode': toUnicode
	};

	/** Expose `punycode` */
	// Some AMD build optimizers, like r.js, check for specific condition patterns
	// like the following:
	if (
		true
	) {
		!(__WEBPACK_AMD_DEFINE_RESULT__ = (function() {
			return punycode;
		}).call(exports, __webpack_require__, exports, module),
				__WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
	} else if (freeExports && freeModule) {
		if (module.exports == freeExports) {
			// in Node.js, io.js, or RingoJS v0.8.0+
			freeModule.exports = punycode;
		} else {
			// in Narwhal or RingoJS v0.7.0-
			for (key in punycode) {
				punycode.hasOwnProperty(key) && (freeExports[key] = punycode[key]);
			}
		}
	} else {
		// in Rhino or a web browser
		root.punycode = punycode;
	}

}(this));

/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(22)(module), __webpack_require__(15)))

/***/ }),

/***/ 6:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_6__;

/***/ }),

/***/ 65:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var EventEmitter = function EventEmitter() {};

module.exports = EventEmitter;

const promise = __webpack_require__(422);
/**
 * Decorate an object with event emitter functionality.
 *
 * @param Object aObjectToDecorate
 *        Bind all public methods of EventEmitter to
 *        the aObjectToDecorate object.
 */


EventEmitter.decorate = function EventEmitter_decorate(aObjectToDecorate) {
  let emitter = new EventEmitter();
  aObjectToDecorate.on = emitter.on.bind(emitter);
  aObjectToDecorate.off = emitter.off.bind(emitter);
  aObjectToDecorate.once = emitter.once.bind(emitter);
  aObjectToDecorate.emit = emitter.emit.bind(emitter);
};

EventEmitter.prototype = {
  /**
   * Connect a listener.
   *
   * @param string aEvent
   *        The event name to which we're connecting.
   * @param function aListener
   *        Called when the event is fired.
   */
  on: function EventEmitter_on(aEvent, aListener) {
    if (!this._eventEmitterListeners) this._eventEmitterListeners = new Map();

    if (!this._eventEmitterListeners.has(aEvent)) {
      this._eventEmitterListeners.set(aEvent, []);
    }

    this._eventEmitterListeners.get(aEvent).push(aListener);
  },

  /**
   * Listen for the next time an event is fired.
   *
   * @param string aEvent
   *        The event name to which we're connecting.
   * @param function aListener
   *        (Optional) Called when the event is fired. Will be called at most
   *        one time.
   * @return promise
   *        A promise which is resolved when the event next happens. The
   *        resolution value of the promise is the first event argument. If
   *        you need access to second or subsequent event arguments (it's rare
   *        that this is needed) then use aListener
   */
  once: function EventEmitter_once(aEvent, aListener) {
    let deferred = promise.defer();

    let handler = (aEvent, aFirstArg, ...aRest) => {
      this.off(aEvent, handler);

      if (aListener) {
        aListener.apply(null, [aEvent, aFirstArg, ...aRest]);
      }

      deferred.resolve(aFirstArg);
    };

    handler._originalListener = aListener;
    this.on(aEvent, handler);
    return deferred.promise;
  },

  /**
   * Remove a previously-registered event listener.  Works for events
   * registered with either on or once.
   *
   * @param string aEvent
   *        The event name whose listener we're disconnecting.
   * @param function aListener
   *        The listener to remove.
   */
  off: function EventEmitter_off(aEvent, aListener) {
    if (!this._eventEmitterListeners) return;

    let listeners = this._eventEmitterListeners.get(aEvent);

    if (listeners) {
      this._eventEmitterListeners.set(aEvent, listeners.filter(l => {
        return l !== aListener && l._originalListener !== aListener;
      }));
    }
  },

  /**
   * Emit an event.  All arguments to this method will
   * be sent to listener functions.
   */
  emit: function EventEmitter_emit(aEvent) {
    if (!this._eventEmitterListeners || !this._eventEmitterListeners.has(aEvent)) {
      return;
    }

    let originalListeners = this._eventEmitterListeners.get(aEvent);

    for (let listener of this._eventEmitterListeners.get(aEvent)) {
      // If the object was destroyed during event emission, stop
      // emitting.
      if (!this._eventEmitterListeners) {
        break;
      } // If listeners were removed during emission, make sure the
      // event handler we're going to fire wasn't removed.


      if (originalListeners === this._eventEmitterListeners.get(aEvent) || this._eventEmitterListeners.get(aEvent).some(l => l === listener)) {
        try {
          listener.apply(null, arguments);
        } catch (ex) {
          // Prevent a bad listener from interfering with the others.
          let msg = ex + ": " + ex.stack; //console.error(msg);

          console.log(msg);
        }
      }
    }
  }
};

/***/ }),

/***/ 66:
/***/ (function(module, exports) {

(function() {
  var AcronymResult, computeScore, emptyAcronymResult, isAcronymFullWord, isMatch, isSeparator, isWordEnd, isWordStart, miss_coeff, pos_bonus, scoreAcronyms, scoreCharacter, scoreConsecutives, scoreExact, scoreExactMatch, scorePattern, scorePosition, scoreSize, tau_size, wm;

  wm = 150;

  pos_bonus = 20;

  tau_size = 150;

  miss_coeff = 0.75;

  exports.score = function(string, query, options) {
    var allowErrors, preparedQuery, score, string_lw;
    preparedQuery = options.preparedQuery, allowErrors = options.allowErrors;
    if (!(allowErrors || isMatch(string, preparedQuery.core_lw, preparedQuery.core_up))) {
      return 0;
    }
    string_lw = string.toLowerCase();
    score = computeScore(string, string_lw, preparedQuery);
    return Math.ceil(score);
  };

  exports.isMatch = isMatch = function(subject, query_lw, query_up) {
    var i, j, m, n, qj_lw, qj_up, si;
    m = subject.length;
    n = query_lw.length;
    if (!m || n > m) {
      return false;
    }
    i = -1;
    j = -1;
    while (++j < n) {
      qj_lw = query_lw.charCodeAt(j);
      qj_up = query_up.charCodeAt(j);
      while (++i < m) {
        si = subject.charCodeAt(i);
        if (si === qj_lw || si === qj_up) {
          break;
        }
      }
      if (i === m) {
        return false;
      }
    }
    return true;
  };

  exports.computeScore = computeScore = function(subject, subject_lw, preparedQuery) {
    var acro, acro_score, align, csc_diag, csc_row, csc_score, csc_should_rebuild, i, j, m, miss_budget, miss_left, n, pos, query, query_lw, record_miss, score, score_diag, score_row, score_up, si_lw, start, sz;
    query = preparedQuery.query;
    query_lw = preparedQuery.query_lw;
    m = subject.length;
    n = query.length;
    acro = scoreAcronyms(subject, subject_lw, query, query_lw);
    acro_score = acro.score;
    if (acro.count === n) {
      return scoreExact(n, m, acro_score, acro.pos);
    }
    pos = subject_lw.indexOf(query_lw);
    if (pos > -1) {
      return scoreExactMatch(subject, subject_lw, query, query_lw, pos, n, m);
    }
    score_row = new Array(n);
    csc_row = new Array(n);
    sz = scoreSize(n, m);
    miss_budget = Math.ceil(miss_coeff * n) + 5;
    miss_left = miss_budget;
    csc_should_rebuild = true;
    j = -1;
    while (++j < n) {
      score_row[j] = 0;
      csc_row[j] = 0;
    }
    i = -1;
    while (++i < m) {
      si_lw = subject_lw[i];
      if (!si_lw.charCodeAt(0) in preparedQuery.charCodes) {
        if (csc_should_rebuild) {
          j = -1;
          while (++j < n) {
            csc_row[j] = 0;
          }
          csc_should_rebuild = false;
        }
        continue;
      }
      score = 0;
      score_diag = 0;
      csc_diag = 0;
      record_miss = true;
      csc_should_rebuild = true;
      j = -1;
      while (++j < n) {
        score_up = score_row[j];
        if (score_up > score) {
          score = score_up;
        }
        csc_score = 0;
        if (query_lw[j] === si_lw) {
          start = isWordStart(i, subject, subject_lw);
          csc_score = csc_diag > 0 ? csc_diag : scoreConsecutives(subject, subject_lw, query, query_lw, i, j, start);
          align = score_diag + scoreCharacter(i, j, start, acro_score, csc_score);
          if (align > score) {
            score = align;
            miss_left = miss_budget;
          } else {
            if (record_miss && --miss_left <= 0) {
              return Math.max(score, score_row[n - 1]) * sz;
            }
            record_miss = false;
          }
        }
        score_diag = score_up;
        csc_diag = csc_row[j];
        csc_row[j] = csc_score;
        score_row[j] = score;
      }
    }
    score = score_row[n - 1];
    return score * sz;
  };

  exports.isWordStart = isWordStart = function(pos, subject, subject_lw) {
    var curr_s, prev_s;
    if (pos === 0) {
      return true;
    }
    curr_s = subject[pos];
    prev_s = subject[pos - 1];
    return isSeparator(prev_s) || (curr_s !== subject_lw[pos] && prev_s === subject_lw[pos - 1]);
  };

  exports.isWordEnd = isWordEnd = function(pos, subject, subject_lw, len) {
    var curr_s, next_s;
    if (pos === len - 1) {
      return true;
    }
    curr_s = subject[pos];
    next_s = subject[pos + 1];
    return isSeparator(next_s) || (curr_s === subject_lw[pos] && next_s !== subject_lw[pos + 1]);
  };

  isSeparator = function(c) {
    return c === ' ' || c === '.' || c === '-' || c === '_' || c === '/' || c === '\\';
  };

  scorePosition = function(pos) {
    var sc;
    if (pos < pos_bonus) {
      sc = pos_bonus - pos;
      return 100 + sc * sc;
    } else {
      return Math.max(100 + pos_bonus - pos, 0);
    }
  };

  exports.scoreSize = scoreSize = function(n, m) {
    return tau_size / (tau_size + Math.abs(m - n));
  };

  scoreExact = function(n, m, quality, pos) {
    return 2 * n * (wm * quality + scorePosition(pos)) * scoreSize(n, m);
  };

  exports.scorePattern = scorePattern = function(count, len, sameCase, start, end) {
    var bonus, sz;
    sz = count;
    bonus = 6;
    if (sameCase === count) {
      bonus += 2;
    }
    if (start) {
      bonus += 3;
    }
    if (end) {
      bonus += 1;
    }
    if (count === len) {
      if (start) {
        if (sameCase === len) {
          sz += 2;
        } else {
          sz += 1;
        }
      }
      if (end) {
        bonus += 1;
      }
    }
    return sameCase + sz * (sz + bonus);
  };

  exports.scoreCharacter = scoreCharacter = function(i, j, start, acro_score, csc_score) {
    var posBonus;
    posBonus = scorePosition(i);
    if (start) {
      return posBonus + wm * ((acro_score > csc_score ? acro_score : csc_score) + 10);
    }
    return posBonus + wm * csc_score;
  };

  exports.scoreConsecutives = scoreConsecutives = function(subject, subject_lw, query, query_lw, i, j, startOfWord) {
    var k, m, mi, n, nj, sameCase, sz;
    m = subject.length;
    n = query.length;
    mi = m - i;
    nj = n - j;
    k = mi < nj ? mi : nj;
    sameCase = 0;
    sz = 0;
    if (query[j] === subject[i]) {
      sameCase++;
    }
    while (++sz < k && query_lw[++j] === subject_lw[++i]) {
      if (query[j] === subject[i]) {
        sameCase++;
      }
    }
    if (sz < k) {
      i--;
    }
    if (sz === 1) {
      return 1 + 2 * sameCase;
    }
    return scorePattern(sz, n, sameCase, startOfWord, isWordEnd(i, subject, subject_lw, m));
  };

  exports.scoreExactMatch = scoreExactMatch = function(subject, subject_lw, query, query_lw, pos, n, m) {
    var end, i, pos2, sameCase, start;
    start = isWordStart(pos, subject, subject_lw);
    if (!start) {
      pos2 = subject_lw.indexOf(query_lw, pos + 1);
      if (pos2 > -1) {
        start = isWordStart(pos2, subject, subject_lw);
        if (start) {
          pos = pos2;
        }
      }
    }
    i = -1;
    sameCase = 0;
    while (++i < n) {
      if (query[pos + i] === subject[i]) {
        sameCase++;
      }
    }
    end = isWordEnd(pos + n - 1, subject, subject_lw, m);
    return scoreExact(n, m, scorePattern(n, n, sameCase, start, end), pos);
  };

  AcronymResult = (function() {
    function AcronymResult(score, pos, count) {
      this.score = score;
      this.pos = pos;
      this.count = count;
    }

    return AcronymResult;

  })();

  emptyAcronymResult = new AcronymResult(0, 0.1, 0);

  exports.scoreAcronyms = scoreAcronyms = function(subject, subject_lw, query, query_lw) {
    var count, fullWord, i, j, m, n, qj_lw, sameCase, score, sepCount, sumPos;
    m = subject.length;
    n = query.length;
    if (!(m > 1 && n > 1)) {
      return emptyAcronymResult;
    }
    count = 0;
    sepCount = 0;
    sumPos = 0;
    sameCase = 0;
    i = -1;
    j = -1;
    while (++j < n) {
      qj_lw = query_lw[j];
      if (isSeparator(qj_lw)) {
        i = subject_lw.indexOf(qj_lw, i + 1);
        if (i > -1) {
          sepCount++;
          continue;
        } else {
          break;
        }
      }
      while (++i < m) {
        if (qj_lw === subject_lw[i] && isWordStart(i, subject, subject_lw)) {
          if (query[j] === subject[i]) {
            sameCase++;
          }
          sumPos += i;
          count++;
          break;
        }
      }
      if (i === m) {
        break;
      }
    }
    if (count < 2) {
      return emptyAcronymResult;
    }
    fullWord = count === n ? isAcronymFullWord(subject, subject_lw, query, count) : false;
    score = scorePattern(count, n, sameCase, true, fullWord);
    return new AcronymResult(score, sumPos / count, count + sepCount);
  };

  isAcronymFullWord = function(subject, subject_lw, query, nbAcronymInQuery) {
    var count, i, m, n;
    m = subject.length;
    n = query.length;
    count = 0;
    if (m > 12 * n) {
      return false;
    }
    i = -1;
    while (++i < m) {
      if (isWordStart(i, subject, subject_lw) && ++count > nbAcronymInQuery) {
        return false;
      }
    }
    return true;
  };

}).call(this);


/***/ }),

/***/ 67:
/***/ (function(module, exports, __webpack_require__) {

var __WEBPACK_AMD_DEFINE_ARRAY__, __WEBPACK_AMD_DEFINE_RESULT__;/*!
  Copyright (c) 2017 Jed Watson.
  Licensed under the MIT License (MIT), see
  http://jedwatson.github.io/classnames
*/
/* global define */

(function () {
	'use strict';

	var hasOwn = {}.hasOwnProperty;

	function classNames () {
		var classes = [];

		for (var i = 0; i < arguments.length; i++) {
			var arg = arguments[i];
			if (!arg) continue;

			var argType = typeof arg;

			if (argType === 'string' || argType === 'number') {
				classes.push(arg);
			} else if (Array.isArray(arg) && arg.length) {
				var inner = classNames.apply(null, arg);
				if (inner) {
					classes.push(inner);
				}
			} else if (argType === 'object') {
				for (var key in arg) {
					if (hasOwn.call(arg, key) && arg[key]) {
						classes.push(key);
					}
				}
			}
		}

		return classes.join(' ');
	}

	if (typeof module !== 'undefined' && module.exports) {
		classNames.default = classNames;
		module.exports = classNames;
	} else if (true) {
		// register as 'classnames', consistent with npm package name
		!(__WEBPACK_AMD_DEFINE_ARRAY__ = [], __WEBPACK_AMD_DEFINE_RESULT__ = (function () {
			return classNames;
		}).apply(exports, __WEBPACK_AMD_DEFINE_ARRAY__),
				__WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
	} else {
		window.classNames = classNames;
	}
}());


/***/ }),

/***/ 7:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const networkRequest = __webpack_require__(13);

const workerUtils = __webpack_require__(14);

module.exports = {
  networkRequest,
  workerUtils
};

/***/ })

/******/ });
});