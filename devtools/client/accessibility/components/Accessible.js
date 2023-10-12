/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global EVENTS, gTelemetry */

// React & Redux
const {
  createFactory,
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  div,
  span,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  findDOMNode,
} = require("resource://devtools/client/shared/vendor/react-dom.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const {
  TREE_ROW_HEIGHT,
  ORDERED_PROPS,
  ACCESSIBLE_EVENTS,
  VALUE_FLASHING_DURATION,
} = require("resource://devtools/client/accessibility/constants.js");
const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");
const {
  flashElementOn,
  flashElementOff,
} = require("resource://devtools/client/inspector/markup/utils.js");
const {
  updateDetails,
} = require("resource://devtools/client/accessibility/actions/details.js");
const {
  select,
  unhighlight,
} = require("resource://devtools/client/accessibility/actions/accessibles.js");

const VirtualizedTree = createFactory(
  require("resource://devtools/client/shared/components/VirtualizedTree.js")
);
// Reps
const {
  REPS,
  MODE,
} = require("resource://devtools/client/shared/components/reps/index.js");
const { Rep, ElementNode, Accessible: AccessibleRep, Obj } = REPS;

const {
  translateNodeFrontToGrip,
} = require("resource://devtools/client/inspector/shared/utils.js");

loader.lazyRequireGetter(
  this,
  "openContentLink",
  "resource://devtools/client/shared/link.js",
  true
);

const TELEMETRY_NODE_INSPECTED_COUNT =
  "devtools.accessibility.node_inspected_count";

const TREE_DEPTH_PADDING_INCREMENT = 20;

class AccessiblePropertyClass extends Component {
  static get propTypes() {
    return {
      accessibleFrontActorID: PropTypes.string,
      object: PropTypes.any,
      focused: PropTypes.bool,
      children: PropTypes.func,
    };
  }

  componentDidUpdate({
    object: prevObject,
    accessibleFrontActorID: prevAccessibleFrontActorID,
  }) {
    const { accessibleFrontActorID, object, focused } = this.props;
    // Fast check if row is focused or if the value did not update.
    if (
      focused ||
      accessibleFrontActorID !== prevAccessibleFrontActorID ||
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
      accessibleFront: PropTypes.object,
      dispatch: PropTypes.func.isRequired,
      nodeFront: PropTypes.object,
      items: PropTypes.array,
      labelledby: PropTypes.string.isRequired,
      parents: PropTypes.object,
      relations: PropTypes.object,
      toolbox: PropTypes.object.isRequired,
      toolboxHighlighter: PropTypes.object.isRequired,
      highlightAccessible: PropTypes.func.isRequired,
      unhighlightAccessible: PropTypes.func.isRequired,
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

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillMount() {
    window.on(
      EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED,
      this.onAccessibleInspected
    );
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps({ accessibleFront }) {
    const oldAccessibleFront = this.props.accessibleFront;

    if (oldAccessibleFront) {
      if (
        accessibleFront &&
        accessibleFront.actorID === oldAccessibleFront.actorID
      ) {
        return;
      }
      ACCESSIBLE_EVENTS.forEach(event =>
        oldAccessibleFront.off(event, this.update)
      );
    }

    if (accessibleFront) {
      ACCESSIBLE_EVENTS.forEach(event =>
        accessibleFront.on(event, this.update)
      );
    }
  }

  componentDidUpdate(prevProps) {
    if (
      this.props.accessibleFront &&
      !this.props.accessibleFront.isDestroyed() &&
      this.props.accessibleFront !== prevProps.accessibleFront
    ) {
      window.emit(EVENTS.PROPERTIES_UPDATED);
    }
  }

  componentWillUnmount() {
    window.off(
      EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED,
      this.onAccessibleInspected
    );

    const { accessibleFront } = this.props;
    if (accessibleFront) {
      ACCESSIBLE_EVENTS.forEach(event =>
        accessibleFront.off(event, this.update)
      );
    }
  }

  onAccessibleInspected() {
    const { props } = this.refs;
    if (props) {
      props.refs.tree.focus();
    }
  }

  update() {
    const { dispatch, accessibleFront } = this.props;
    if (accessibleFront.isDestroyed()) {
      return;
    }

    dispatch(updateDetails(accessibleFront));
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
    if (!this.props.toolboxHighlighter) {
      return;
    }

    await this.props.toolboxHighlighter.highlight(nodeFront);
  }

  async hideHighlighter() {
    if (!this.props.toolboxHighlighter) {
      return;
    }

    await this.props.toolboxHighlighter.unhighlight();
  }

  showAccessibleHighlighter(accessibleFront) {
    this.props.dispatch(unhighlight());
    this.props.highlightAccessible(accessibleFront);
  }

  hideAccessibleHighlighter(accessibleFront) {
    this.props.dispatch(unhighlight());
    this.props.unhighlightAccessible(accessibleFront);
  }

  async selectNode(nodeFront, reason = "accessibility") {
    if (gTelemetry) {
      gTelemetry.scalarAdd(TELEMETRY_NODE_INSPECTED_COUNT, 1);
    }

    if (!this.props.toolbox) {
      return;
    }

    const inspector = await this.props.toolbox.selectTool("inspector");
    inspector.selection.setNodeFront(nodeFront, reason);
  }

  async selectAccessible(accessibleFront) {
    if (!accessibleFront) {
      return;
    }

    await this.props.dispatch(select(accessibleFront));

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

    if (isNodeFront(object)) {
      valueProps.defaultRep = ElementNode;
      valueProps.onDOMNodeMouseOut = () => this.hideHighlighter();
      valueProps.onDOMNodeMouseOver = () =>
        this.showHighlighter(this.props.nodeFront);

      valueProps.inspectIconTitle = L10N.getStr(
        "accessibility.accessible.selectNodeInInspector.title"
      );
      valueProps.onInspectIconClick = () =>
        this.selectNode(this.props.nodeFront);
    } else if (isAccessibleFront(object)) {
      const target = findAccessibleTarget(this.props.relations, object.actor);
      valueProps.defaultRep = AccessibleRep;
      valueProps.onAccessibleMouseOut = () =>
        this.hideAccessibleHighlighter(target);
      valueProps.onAccessibleMouseOver = () =>
        this.showAccessibleHighlighter(target);
      valueProps.inspectIconTitle = L10N.getStr(
        "accessibility.accessible.selectElement.title"
      );
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
      {
        object,
        focused,
        accessibleFrontActorID: this.props.accessibleFront.actorID,
      },
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
    const { items, parents, accessibleFront, labelledby } = this.props;

    if (accessibleFront) {
      return VirtualizedTree({
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
 * Check if a given property is a DOMNode front.
 * @param  {Object?} value A property to check for being a DOMNode.
 * @return {Boolean}       A flag that indicates whether a property is a DOMNode.
 */
const isNodeFront = value => value && value.typeName === "domnode";

/**
 * Check if a given property is an Accessible front.
 * @param  {Object?} value A property to check for being an Accessible.
 * @return {Boolean}       A flag that indicates whether a property is an Accessible.
 */
const isAccessibleFront = value => value && value.typeName === "accessible";

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
 * Build props ingestible by VirtualizedTree component.
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
      if (isNodeFront(contents)) {
        contents = translateNodeFrontToGripWrapper(contents);
        name = "DOMNode";
      } else if (isAccessibleFront(contents)) {
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
    if (item.children.length) {
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
  const {
    accessible: accessibleFront,
    DOMNode: nodeFront,
    relations,
  } = details;
  if (!accessibleFront || !nodeFront) {
    return {};
  }

  const items = makeItemsForDetails(
    ORDERED_PROPS.reduce((props, key) => {
      if (key === "DOMNode") {
        props.nodeFront = nodeFront;
      } else if (key === "relations") {
        props.relations = relations;
      } else {
        props[key] = accessibleFront[key];
      }

      return props;
    }, {}),
    ""
  );
  const parents = makeParentMap(items);

  return { accessibleFront, nodeFront, items, parents, relations };
};

module.exports = connect(mapStateToProps)(Accessible);
