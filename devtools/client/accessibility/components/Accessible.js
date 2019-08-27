/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global EVENTS, gTelemetry, gToolbox */

// React & Redux
const {
  createFactory,
  Component,
} = require("devtools/client/shared/vendor/react");
const {
  div,
  span,
} = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const {
  TREE_ROW_HEIGHT,
  ORDERED_PROPS,
  ACCESSIBLE_EVENTS,
  VALUE_FLASHING_DURATION,
} = require("../constants");
const { L10N } = require("../utils/l10n");
const {
  flashElementOn,
  flashElementOff,
} = require("devtools/client/inspector/markup/utils");
const { updateDetails } = require("../actions/details");
const { select, unhighlight } = require("../actions/accessibles");

const Tree = createFactory(
  require("devtools/client/shared/components/VirtualizedTree")
);
// Reps
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep, ElementNode, Accessible: AccessibleRep, Obj } = REPS;

const {
  translateNodeFrontToGrip,
} = require("devtools/client/inspector/shared/utils");

loader.lazyRequireGetter(
  this,
  "openContentLink",
  "devtools/client/shared/link",
  true
);

const TELEMETRY_NODE_INSPECTED_COUNT =
  "devtools.accessibility.node_inspected_count";

const TREE_DEPTH_PADDING_INCREMENT = 20;

class AccessiblePropertyClass extends Component {
  static get propTypes() {
    return {
      accessible: PropTypes.string,
      object: PropTypes.any,
      focused: PropTypes.bool,
      children: PropTypes.func,
    };
  }

  componentDidUpdate({ object: prevObject, accessible: prevAccessible }) {
    const { accessible, object, focused } = this.props;
    // Fast check if row is focused or if the value did not update.
    if (
      focused ||
      accessible !== prevAccessible ||
      prevObject === object ||
      (object && prevObject && typeof object === "object")
    ) {
      return;
    }

    this.flashRow();
  }

  flashRow() {
    const row = findDOMNode(this);
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
      parents: PropTypes.object,
      relations: PropTypes.object,
      supports: PropTypes.object,
      accessibilityWalker: PropTypes.object.isRequired,
      getDOMWalker: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      expanded: new Set(),
      active: null,
      focused: null,
    };

    this.onAccessibleInspected = this.onAccessibleInspected.bind(this);
    this.renderItem = this.renderItem.bind(this);
    this.update = this.update.bind(this);
  }

  componentWillMount() {
    window.on(
      EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED,
      this.onAccessibleInspected
    );
  }

  componentWillReceiveProps({ accessible }) {
    const oldAccessible = this.props.accessible;

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
    window.off(
      EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED,
      this.onAccessibleInspected
    );

    const { accessible } = this.props;
    if (accessible) {
      ACCESSIBLE_EVENTS.forEach(event => accessible.off(event, this.update));
    }
  }

  onAccessibleInspected() {
    const { props } = this.refs;
    if (props) {
      props.refs.tree.focus();
    }
  }

  async update() {
    const { dispatch, accessible, supports, getDOMWalker } = this.props;
    const domWalker = await getDOMWalker();
    if (!domWalker || !accessible.actorID) {
      return;
    }

    dispatch(updateDetails(domWalker, accessible, supports));
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

  async showHighlighter(nodeFront) {
    if (!gToolbox) {
      return;
    }

    const { highlighterFront } = nodeFront;
    await highlighterFront.highlight(nodeFront);
  }

  async hideHighlighter(nodeFront) {
    if (!gToolbox) {
      return;
    }

    const { highlighterFront } = nodeFront;
    await highlighterFront.unhighlight();
  }

  showAccessibleHighlighter(accessible) {
    const { accessibilityWalker, dispatch } = this.props;
    dispatch(unhighlight());

    if (!accessible || !accessibilityWalker) {
      return;
    }

    accessibilityWalker.highlightAccessible(accessible).catch(error => {
      // Only report an error where there's still a toolbox. Ignore cases where toolbox is
      // already destroyed.
      if (gToolbox) {
        console.error(error);
      }
    });
  }

  hideAccessibleHighlighter() {
    const { accessibilityWalker, dispatch } = this.props;
    dispatch(unhighlight());

    if (!accessibilityWalker) {
      return;
    }

    accessibilityWalker.unhighlight().catch(error => {
      // Only report an error where there's still a toolbox. Ignore cases where toolbox is
      // already destroyed.
      if (gToolbox) {
        console.error(error);
      }
    });
  }

  selectNode(nodeFront, reason = "accessibility") {
    if (gTelemetry) {
      gTelemetry.scalarAdd(TELEMETRY_NODE_INSPECTED_COUNT, 1);
    }

    if (!gToolbox) {
      return;
    }

    gToolbox
      .selectTool("inspector")
      .then(() => gToolbox.selection.setNodeFront(nodeFront, reason));
  }

  async selectAccessible(accessible) {
    const { accessibilityWalker, dispatch } = this.props;
    if (!accessibilityWalker) {
      return;
    }

    await dispatch(select(accessibilityWalker, accessible));

    const { props } = this.refs;
    if (props) {
      props.refs.tree.blur();
    }
    await this.setState({ active: null, focused: null });

    window.emit(EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED);
  }

  openLink(link, e) {
    openContentLink(link);
  }

  renderItem(item, depth, focused, arrow, expanded) {
    const object = item.contents;
    const valueProps = {
      object,
      mode: MODE.TINY,
      title: "Object",
      openLink: this.openLink,
    };

    if (isNode(object)) {
      valueProps.defaultRep = ElementNode;
      valueProps.onDOMNodeMouseOut = () =>
        this.hideHighlighter(this.props.DOMNode);
      valueProps.onDOMNodeMouseOver = () =>
        this.showHighlighter(this.props.DOMNode);
      valueProps.onInspectIconClick = () => this.selectNode(this.props.DOMNode);
    } else if (isAccessible(object)) {
      const target = findAccessibleTarget(this.props.relations, object.actor);
      valueProps.defaultRep = AccessibleRep;
      valueProps.onAccessibleMouseOut = () => this.hideAccessibleHighlighter();
      valueProps.onAccessibleMouseOver = () =>
        this.showAccessibleHighlighter(target);
      valueProps.onInspectIconClick = (obj, e) => {
        e.stopPropagation();
        this.selectAccessible(target);
      };
      valueProps.separatorText = "";
    } else if (item.name === "relations") {
      valueProps.defaultRep = Obj;
    } else {
      valueProps.noGrip = true;
    }

    const classList = ["node", "object-node"];
    if (focused) {
      classList.push("focused");
    }

    const depthPadding = depth * TREE_DEPTH_PADDING_INCREMENT;

    return AccessibleProperty(
      { object, focused, accessible: this.props.accessible.actorID },
      () =>
        div(
          {
            className: classList.join(" "),
            style: {
              paddingInlineStart: depthPadding,
              inlineSize: `calc(var(--accessibility-properties-item-width) - ${depthPadding}px)`,
            },
            onClick: e => {
              if (e.target.classList.contains("theme-twisty")) {
                this.setExpanded(item, !expanded);
              }
            },
          },
          arrow,
          span({ className: "object-label" }, item.name),
          span({ className: "object-delimiter" }, ":"),
          span({ className: "object-value" }, Rep(valueProps) || "")
        )
    );
  }

  render() {
    const { expanded, active, focused } = this.state;
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
        onActivate: item => {
          if (item == null) {
            this.setState({ active: null });
          } else if (this.state.active !== item.path) {
            this.setState({ active: item.path });
          }
        },
        focused: findByPath(focused, items),
        active: findByPath(active, items),
        renderItem: this.renderItem,
        labelledby,
      });
    }

    return div(
      { className: "info" },
      L10N.getStr("accessibility.accessible.notAvailable")
    );
  }
}

/**
 * Match accessibility object from relations targets to the grip that's being activated.
 * @param  {Object} relations  Object containing relations grouped by type and targets.
 * @param  {String} actorID    Actor ID to match to the relation target.
 * @return {Object}            Accessible front that matches the relation target.
 */
const findAccessibleTarget = (relations, actorID) => {
  for (const relationType in relations) {
    let targets = relations[relationType];
    targets = Array.isArray(targets) ? targets : [targets];
    for (const target of targets) {
      if (target.actorID === actorID) {
        return target;
      }
    }
  }

  return null;
};

/**
 * Find an item based on a given path.
 * @param  {String} path
 *         Key of the item to be looked up.
 * @param  {Array}  items
 *         Accessibility properties array.
 * @return {Object?}
 *         Possibly found item.
 */
const findByPath = (path, items) => {
  for (const item of items) {
    if (item.path === path) {
      return item;
    }

    const found = findByPath(path, item.children);
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
 * Check if a given property is an Accessible actor.
 * @param  {Object?} value A property to check for being an Accessible.
 * @return {Boolean}       A flag that indicates whether a property is an Accessible.
 */
const isAccessible = value => value && value.typeName === "accessible";

/**
 * While waiting for a reps fix in https://github.com/firefox-devtools/reps/issues/92,
 * translate accessibleFront to a grip-like object that can be used with an Accessible
 * rep.
 *
 * @params  {accessibleFront} accessibleFront
 *          The AccessibleFront for which we want to create a grip-like object.
 * @returns {Object} a grip-like object that can be used with Reps.
 */
const translateAccessibleFrontToGrip = accessibleFront => ({
  actor: accessibleFront.actorID,
  typeName: accessibleFront.typeName,
  preview: {
    name: accessibleFront.name,
    role: accessibleFront.role,
    // All the grid containers are assumed to be in the Accessibility tree.
    isConnected: true,
  },
});

const translateNodeFrontToGripWrapper = nodeFront => ({
  ...translateNodeFrontToGrip(nodeFront),
  typeName: nodeFront.typeName,
});

/**
 * Build props ingestible by Tree component.
 * @param  {Object} props      Component properties to be processed.
 * @param  {String} parentPath Unique path that is used to identify a Tree Node.
 * @return {Object}            Processed properties.
 */
const makeItemsForDetails = (props, parentPath) =>
  Object.getOwnPropertyNames(props).map(name => {
    let children = [];
    const path = `${parentPath}/${name}`;
    let contents = props[name];

    if (contents) {
      if (isNode(contents)) {
        contents = translateNodeFrontToGripWrapper(contents);
      } else if (isAccessible(contents)) {
        contents = translateAccessibleFrontToGrip(contents);
      } else if (Array.isArray(contents) || typeof contents === "object") {
        children = makeItemsForDetails(contents, path);
      }
    }

    return { name, path, contents, children };
  });

const makeParentMap = items => {
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

const mapStateToProps = ({ details, ui }) => {
  const { accessible, DOMNode, relations } = details;
  const { supports } = ui;
  if (!accessible || !DOMNode) {
    return {};
  }

  const items = makeItemsForDetails(
    ORDERED_PROPS.reduce((props, key) => {
      if (key === "DOMNode") {
        props.DOMNode = DOMNode;
      } else if (key === "relations") {
        if (supports.relations) {
          props.relations = relations;
        }
      } else {
        props[key] = accessible[key];
      }

      return props;
    }, {}),
    ""
  );
  const parents = makeParentMap(items);

  return { accessible, DOMNode, items, parents, relations, supports };
};

module.exports = connect(mapStateToProps)(Accessible);
