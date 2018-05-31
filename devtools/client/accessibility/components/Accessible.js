/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global EVENTS, gTelemetry, gToolbox */

// React & Redux
const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const { div, span } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { TREE_ROW_HEIGHT, ORDERED_PROPS, ACCESSIBLE_EVENTS, VALUE_FLASHING_DURATION } =
  require("../constants");
const { L10N } = require("../utils/l10n");
const {flashElementOn, flashElementOff} =
      require("devtools/client/inspector/markup/utils");
const { updateDetails } = require("../actions/details");

const Tree = createFactory(require("devtools/client/shared/components/VirtualizedTree"));
// Reps
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep, ElementNode } = REPS;

const TELEMETRY_NODE_INSPECTED_COUNT = "devtools.accessibility.node_inspected_count";

class AccessiblePropertyClass extends Component {
  static get propTypes() {
    return {
      accessible: PropTypes.string,
      object: PropTypes.any,
      focused: PropTypes.bool,
      children: PropTypes.func
    };
  }

  componentDidUpdate({ object: prevObject, accessible: prevAccessible }) {
    let { accessible, object, focused } = this.props;
    // Fast check if row is focused or if the value did not update.
    if (focused || accessible !== prevAccessible || prevObject === object ||
        (object && prevObject && typeof object === "object")) {
      return;
    }

    this.flashRow();
  }

  flashRow() {
    let row = findDOMNode(this);
    flashElementOn(row);
    if (this._flashMutationTimer) {
      clearTimeout(this._flashMutationTimer);
      this._flashMutationTimer = null;
    }
    this._flashMutationTimer = setTimeout(() => {
      flashElementOff(row);
    }, VALUE_FLASHING_DURATION);
  }

  render() {
    return this.props.children();
  }
}

const AccessibleProperty = createFactory(AccessiblePropertyClass);

class Accessible extends Component {
  static get propTypes() {
    return {
      accessible: PropTypes.object,
      dispatch: PropTypes.func.isRequired,
      DOMNode: PropTypes.object,
      items: PropTypes.array,
      labelledby: PropTypes.string.isRequired,
      parents: PropTypes.object
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      expanded: new Set(),
      focused: null
    };

    this.onAccessibleInspected = this.onAccessibleInspected.bind(this);
    this.renderItem = this.renderItem.bind(this);
    this.update = this.update.bind(this);
  }

  componentWillMount() {
    window.on(EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED, this.onAccessibleInspected);
  }

  componentWillReceiveProps({ accessible }) {
    let oldAccessible = this.props.accessible;

    if (oldAccessible) {
      if (accessible && accessible.actorID === oldAccessible.actorID) {
        return;
      }
      ACCESSIBLE_EVENTS.forEach(event => oldAccessible.off(event, this.update));
    }

    if (accessible) {
      ACCESSIBLE_EVENTS.forEach(event => accessible.on(event, this.update));
    }
  }

  componentWillUnmount() {
    window.off(EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED, this.onAccessibleInspected);

    let { accessible } = this.props;
    if (accessible) {
      ACCESSIBLE_EVENTS.forEach(event => accessible.off(event, this.update));
    }
  }

  onAccessibleInspected() {
    let { props } = this.refs;
    if (props) {
      props.refs.tree.focus();
    }
  }

  update() {
    let { dispatch, accessible } = this.props;
    if (gToolbox) {
      dispatch(updateDetails(gToolbox.walker, accessible));
    }
  }

  setExpanded(item, isExpanded) {
    const { expanded } = this.state;

    if (isExpanded) {
      expanded.add(item.path);
    } else {
      expanded.delete(item.path);
    }

    this.setState({ expanded });
  }

  showHighlighter(nodeFront) {
    if (!gToolbox) {
      return;
    }

    gToolbox.highlighterUtils.highlightNodeFront(nodeFront);
  }

  hideHighlighter() {
    if (!gToolbox) {
      return;
    }

    gToolbox.highlighterUtils.unhighlight();
  }

  selectNode(nodeFront, reason = "accessibility") {
    if (gTelemetry) {
      gTelemetry.scalarAdd(TELEMETRY_NODE_INSPECTED_COUNT, 1);
    }

    if (!gToolbox) {
      return;
    }

    gToolbox.selectTool("inspector").then(() =>
      gToolbox.selection.setNodeFront(nodeFront, reason));
  }

  openLink(link, e) {
    if (!gToolbox) {
      return;
    }

    // Avoid using Services.appinfo.OS in order to keep accessible pane's frontend free of
    // priveleged code.
    const os = window.navigator.userAgent;
    const isOSX =  os && os.includes("Mac");
    let where = "tab";
    if (e && (e.button === 1 || (e.button === 0 && (isOSX ? e.metaKey : e.ctrlKey)))) {
      where = "tabshifted";
    } else if (e && e.shiftKey) {
      where = "window";
    }

    const win = gToolbox.doc.defaultView.top;
    win.openWebLinkIn(link, where);
  }

  renderItem(item, depth, focused, arrow, expanded) {
    const object = item.contents;
    let valueProps = {
      object,
      mode: MODE.TINY,
      title: "Object",
      openLink: this.openLink
    };

    if (isNode(object)) {
      valueProps.defaultRep = ElementNode;
      valueProps.onDOMNodeMouseOut = () => this.hideHighlighter();
      valueProps.onDOMNodeMouseOver = () => this.showHighlighter(this.props.DOMNode);
      valueProps.onInspectIconClick = () => this.selectNode(this.props.DOMNode);
    }

    let classList = [ "node", "object-node" ];
    if (focused) {
      classList.push("focused");
    }

    return AccessibleProperty(
      { object, focused, accessible: this.props.accessible.actorID },
      () => div({
        className: classList.join(" "),
        style: { paddingInlineStart: depth * 15 },
        onClick: e => {
          if (e.target.classList.contains("theme-twisty")) {
            this.setExpanded(item, !expanded);
          }
        }
      },
        arrow,
        span({ className: "object-label" }, item.name),
        span({ className: "object-delimiter" }, ":"),
        span({ className: "object-value" }, Rep(valueProps) || "")
      )
    );
  }

  render() {
    const { expanded, focused } = this.state;
    const { items, parents, accessible, labelledby } = this.props;

    if (accessible) {
      return Tree({
        ref: "props",
        key: "accessible-properties",
        itemHeight: TREE_ROW_HEIGHT,
        getRoots: () => items,
        getKey: item => item.path,
        getParent: item => parents.get(item),
        getChildren: item => item.children,
        isExpanded: item => expanded.has(item.path),
        onExpand: item => this.setExpanded(item, true),
        onCollapse: item => this.setExpanded(item, false),
        onFocus: item => {
          if (this.state.focused !== item.path) {
            this.setState({ focused: item.path });
          }
        },
        onActivate: ({ contents }) => {
          if (isNode(contents)) {
            this.selectNode(this.props.DOMNode, "accessibility-keyboard");
          }
        },
        focused: findFocused(focused, items),
        renderItem: this.renderItem,
        labelledby
      });
    }

    return div({ className: "info" },
               L10N.getStr("accessibility.accessible.notAvailable"));
  }
}

/**
 * Find currently focused item.
 * @param  {String} focused Key of the currently focused item.
 * @param  {Array}  items   Accessibility properties array.
 * @return {Object?}        Possibly found focused item.
 */
const findFocused = (focused, items) => {
  for (let item of items) {
    if (item.path === focused) {
      return item;
    }
    let found = findFocused(focused, item.children);
    if (found) {
      return found;
    }
  }
  return null;
};

/**
 * Check if a given property is a DOMNode actor.
 * @param  {Object?} value A property to check for being a DOMNode.
 * @return {Boolean}       A flag that indicates whether a property is a DOMNode.
 */
const isNode = value => value && value.typeName === "domnode";

/**
 * While waiting for a reps fix in https://github.com/devtools-html/reps/issues/92,
 * translate nodeFront to a grip-like object that can be used with an ElementNode rep.
 *
 * @params  {NodeFront} nodeFront
 *          The NodeFront for which we want to create a grip-like object.
 * @returns {Object} a grip-like object that can be used with Reps.
 */
const translateNodeFrontToGrip = nodeFront => {
  let { attributes, actorID, typeName, nodeName, nodeType } = nodeFront;

  // The main difference between NodeFront and grips is that attributes are treated as
  // a map in grips and as an array in NodeFronts.
  let attributesMap = {};
  for (let { name, value } of attributes) {
    attributesMap[name] = value;
  }

  return {
    actor: actorID,
    typeName,
    preview: {
      attributes: attributesMap,
      attributesLength: attributes.length,
      // All the grid containers are assumed to be in the DOM tree.
      isConnected: true,
      // nodeName is already lowerCased in Node grips
      nodeName: nodeName.toLowerCase(),
      nodeType
    }
  };
};

/**
 * Build props ingestible by Tree component.
 * @param  {Object} props      Component properties to be processed.
 * @param  {String} parentPath Unique path that is used to identify a Tree Node.
 * @return {Object}            Processed properties.
 */
const makeItemsForDetails = (props, parentPath) =>
  Object.getOwnPropertyNames(props).map(name => {
    let children = [];
    let path = `${parentPath}/${name}`;
    let contents = props[name];

    if (contents) {
      if (isNode(contents)) {
        contents = translateNodeFrontToGrip(contents);
      } else if (Array.isArray(contents) || typeof contents === "object") {
        children = makeItemsForDetails(contents, path);
      }
    }

    return { name, path, contents, children };
  });

const makeParentMap = (items) => {
  const map = new WeakMap();

  function _traverse(item) {
    if (item.children.length > 0) {
      for (const child of item.children) {
        map.set(child, item);
        _traverse(child);
      }
    }
  }

  items.forEach(_traverse);
  return map;
};

const mapStateToProps = ({ details }) => {
  let { accessible, DOMNode } = details;
  if (!accessible || !DOMNode) {
    return {};
  }

  let items = makeItemsForDetails(ORDERED_PROPS.reduce((props, key) => {
    props[key] = key === "DOMNode" ? DOMNode : accessible[key];
    return props;
  }, {}), "");
  let parents = makeParentMap(items);

  return { accessible, DOMNode, items, parents };
};

module.exports = connect(mapStateToProps)(Accessible);
