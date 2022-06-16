/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  Component,
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const {
  connect,
  Provider,
} = require("devtools/client/shared/vendor/react-redux");
loader.lazyRequireGetter(
  this,
  "createStore",
  "devtools/client/shared/redux/create-store"
);

const actions = require("devtools/client/shared/components/object-inspector/actions");
const {
  getExpandedPaths,
  getLoadedProperties,
  getEvaluations,
  default: reducer,
} = require("devtools/client/shared/components/object-inspector/reducer");

const Tree = createFactory(require("devtools/client/shared/components/Tree"));

const ObjectInspectorItem = createFactory(
  require("devtools/client/shared/components/object-inspector/components/ObjectInspectorItem")
);

const Utils = require("devtools/client/shared/components/object-inspector/utils/index");
const { renderRep, shouldRenderRootsInReps } = Utils;
const {
  getChildrenWithEvaluations,
  getActor,
  getEvaluatedItem,
  getParent,
  getValue,
  nodeIsPrimitive,
  nodeHasGetter,
  nodeHasSetter,
} = Utils.node;

// This implements a component that renders an interactive inspector
// for looking at JavaScript objects. It expects descriptions of
// objects from the protocol, and will dynamically fetch children
// properties as objects are expanded.
//
// If you want to inspect a single object, pass the name and the
// protocol descriptor of it:
//
//  ObjectInspector({
//    name: "foo",
//    desc: { writable: true, ..., { value: { actor: "1", ... }}},
//    ...
//  })
//
// If you want multiple top-level objects (like scopes), you can pass
// an array of manually constructed nodes as `roots`:
//
//  ObjectInspector({
//    roots: [{ name: ... }, ...],
//    ...
//  });

// There are 3 types of nodes: a simple node with a children array, an
// object that has properties that should be children when they are
// fetched, and a primitive value that should be displayed with no
// children.

class ObjectInspector extends Component {
  static defaultProps;
  constructor(props) {
    super();
    this.cachedNodes = new Map();

    const self = this;

    self.getItemChildren = this.getItemChildren.bind(this);
    self.isNodeExpandable = this.isNodeExpandable.bind(this);
    self.setExpanded = this.setExpanded.bind(this);
    self.focusItem = this.focusItem.bind(this);
    self.activateItem = this.activateItem.bind(this);
    self.getRoots = this.getRoots.bind(this);
    self.getNodeKey = this.getNodeKey.bind(this);
    self.shouldItemUpdate = this.shouldItemUpdate.bind(this);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillMount() {
    this.roots = this.props.roots;
    this.focusedItem = this.props.focusedItem;
    this.activeItem = this.props.activeItem;
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillUpdate(nextProps) {
    this.removeOutdatedNodesFromCache(nextProps);

    if (this.roots !== nextProps.roots) {
      // Since the roots changed, we assume the properties did as well,
      // so we need to cleanup the component internal state.
      this.roots = nextProps.roots;
      this.focusedItem = nextProps.focusedItem;
      this.activeItem = nextProps.activeItem;
      if (this.props.rootsChanged) {
        this.props.rootsChanged(this.roots);
      }
    }
  }

  removeOutdatedNodesFromCache(nextProps) {
    // When the roots changes, we can wipe out everything.
    if (this.roots !== nextProps.roots) {
      this.cachedNodes.clear();
      return;
    }

    for (const [path, properties] of nextProps.loadedProperties) {
      if (properties !== this.props.loadedProperties.get(path)) {
        this.cachedNodes.delete(path);
      }
    }

    // If there are new evaluations, we want to remove the existing cached
    // nodes from the cache.
    if (nextProps.evaluations > this.props.evaluations) {
      for (const key of nextProps.evaluations.keys()) {
        if (!this.props.evaluations.has(key)) {
          this.cachedNodes.delete(key);
        }
      }
    }
  }

  shouldComponentUpdate(nextProps) {
    const { expandedPaths, loadedProperties, evaluations } = this.props;

    // We should update if:
    // - there are new loaded properties
    // - OR there are new evaluations
    // - OR the expanded paths number changed, and all of them have properties
    //      loaded
    // - OR the expanded paths number did not changed, but old and new sets
    //      differ
    // - OR the focused node changed.
    // - OR the active node changed.
    return (
      loadedProperties !== nextProps.loadedProperties ||
      loadedProperties.size !== nextProps.loadedProperties.size ||
      evaluations.size !== nextProps.evaluations.size ||
      (expandedPaths.size !== nextProps.expandedPaths.size &&
        [...nextProps.expandedPaths].every(path =>
          nextProps.loadedProperties.has(path)
        )) ||
      (expandedPaths.size === nextProps.expandedPaths.size &&
        [...nextProps.expandedPaths].some(key => !expandedPaths.has(key))) ||
      this.focusedItem !== nextProps.focusedItem ||
      this.activeItem !== nextProps.activeItem ||
      this.roots !== nextProps.roots
    );
  }

  componentWillUnmount() {
    this.props.closeObjectInspector(this.props.roots);
  }

  getItemChildren(item) {
    const { loadedProperties, evaluations } = this.props;
    const { cachedNodes } = this;

    return getChildrenWithEvaluations({
      evaluations,
      loadedProperties,
      cachedNodes,
      item,
    });
  }

  getRoots() {
    const { evaluations, roots } = this.props;
    const length = roots.length;

    for (let i = 0; i < length; i++) {
      let rootItem = roots[i];

      if (evaluations.has(rootItem.path)) {
        roots[i] = getEvaluatedItem(rootItem, evaluations);
      }
    }

    return roots;
  }

  getNodeKey(item) {
    return item.path && typeof item.path.toString === "function"
      ? item.path.toString()
      : JSON.stringify(item);
  }

  isNodeExpandable(item) {
    if (
      nodeIsPrimitive(item) ||
      (Array.isArray(item.contents?.value?.header) &&
        !item.contents?.value?.hasBody)
    ) {
      return false;
    }

    if (nodeHasSetter(item) || nodeHasGetter(item)) {
      return false;
    }

    return true;
  }

  setExpanded(item, expand) {
    if (!this.isNodeExpandable(item)) {
      return;
    }

    const {
      nodeExpand,
      nodeCollapse,
      recordTelemetryEvent,
      setExpanded,
      roots,
    } = this.props;

    if (expand === true) {
      const actor = getActor(item, roots);
      nodeExpand(item, actor);
      if (recordTelemetryEvent) {
        recordTelemetryEvent("object_expanded");
      }
    } else {
      nodeCollapse(item);
    }

    if (setExpanded) {
      setExpanded(item, expand);
    }
  }

  focusItem(item) {
    const { focusable = true, onFocus } = this.props;

    if (focusable && this.focusedItem !== item) {
      this.focusedItem = item;
      this.forceUpdate();

      if (onFocus) {
        onFocus(item);
      }
    }
  }

  activateItem(item) {
    const { focusable = true, onActivate } = this.props;

    if (focusable && this.activeItem !== item) {
      this.activeItem = item;
      this.forceUpdate();

      if (onActivate) {
        onActivate(item);
      }
    }
  }

  shouldItemUpdate(prevItem, nextItem) {
    const value = getValue(nextItem);
    // Long string should always update because fullText loading will not
    // trigger item re-render.
    return value && value.type === "longString";
  }

  render() {
    const {
      autoExpandAll = true,
      autoExpandDepth = 1,
      initiallyExpanded,
      focusable = true,
      disableWrap = false,
      expandedPaths,
      inline,
    } = this.props;

    const classNames = ["object-inspector"];
    if (inline) {
      classNames.push("inline");
    }
    if (disableWrap) {
      classNames.push("nowrap");
    }

    return Tree({
      className: classNames.join(" "),

      autoExpandAll,
      autoExpandDepth,
      initiallyExpanded,
      isExpanded: item => expandedPaths && expandedPaths.has(item.path),
      isExpandable: this.isNodeExpandable,
      focused: this.focusedItem,
      active: this.activeItem,

      getRoots: this.getRoots,
      getParent,
      getChildren: this.getItemChildren,
      getKey: this.getNodeKey,

      onExpand: item => this.setExpanded(item, true),
      onCollapse: item => this.setExpanded(item, false),
      onFocus: focusable ? this.focusItem : null,
      onActivate: focusable ? this.activateItem : null,

      shouldItemUpdate: this.shouldItemUpdate,
      renderItem: (item, depth, focused, arrow, expanded) =>
        ObjectInspectorItem({
          ...this.props,
          item,
          depth,
          focused,
          arrow,
          expanded,
          setExpanded: this.setExpanded,
        }),
    });
  }
}

function mapStateToProps(state, props) {
  return {
    expandedPaths: getExpandedPaths(state),
    loadedProperties: getLoadedProperties(state),
    evaluations: getEvaluations(state),
  };
}

const OI = connect(mapStateToProps, actions)(ObjectInspector);

module.exports = props => {
  const { roots, standalone = false } = props;

  if (roots.length == 0) {
    return null;
  }

  if (shouldRenderRootsInReps(roots, props)) {
    return renderRep(roots[0], props);
  }

  const oiElement = createElement(OI, props);

  if (!standalone) {
    return oiElement;
  }

  const store = createStore(reducer);
  return createElement(Provider, { store }, oiElement);
};
