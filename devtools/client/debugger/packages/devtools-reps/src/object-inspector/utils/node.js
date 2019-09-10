/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

const { maybeEscapePropertyName } = require("../../reps/rep-utils");
const ArrayRep = require("../../reps/array");
const GripArrayRep = require("../../reps/grip-array");
const GripMap = require("../../reps/grip-map");
const GripMapEntryRep = require("../../reps/grip-map-entry");
const ErrorRep = require("../../reps/error");
const BigIntRep = require("../../reps/big-int");
const { isLongString } = require("../../reps/string");

const MAX_NUMERICAL_PROPERTIES = 100;

const NODE_TYPES = {
  BUCKET: Symbol("[n…m]"),
  DEFAULT_PROPERTIES: Symbol("<default properties>"),
  ENTRIES: Symbol("<entries>"),
  GET: Symbol("<get>"),
  GRIP: Symbol("GRIP"),
  MAP_ENTRY_KEY: Symbol("<key>"),
  MAP_ENTRY_VALUE: Symbol("<value>"),
  PROMISE_REASON: Symbol("<reason>"),
  PROMISE_STATE: Symbol("<state>"),
  PROMISE_VALUE: Symbol("<value>"),
  PROXY_HANDLER: Symbol("<handler>"),
  PROXY_TARGET: Symbol("<target>"),
  SET: Symbol("<set>"),
  PROTOTYPE: Symbol("<prototype>"),
  BLOCK: Symbol("☲"),
};

import type {
  CachedNodes,
  GripProperties,
  LoadedProperties,
  Node,
  ObjectInspectorItemContentsValue,
  RdpGrip,
} from "../types";

let WINDOW_PROPERTIES = {};

if (typeof window === "object") {
  WINDOW_PROPERTIES = Object.getOwnPropertyNames(window);
}

function getType(item: Node): Symbol {
  return item.type;
}

function getValue(item: Node): RdpGrip | ObjectInspectorItemContentsValue {
  if (nodeHasValue(item)) {
    return item.contents.value;
  }

  if (nodeHasGetterValue(item)) {
    return item.contents.getterValue;
  }

  if (nodeHasAccessors(item)) {
    return item.contents;
  }

  return undefined;
}

function getActor(item: Node, roots) {
  const isRoot = isNodeRoot(item, roots);
  const value = getValue(item);
  return isRoot || !value ? null : value.actor;
}

function isNodeRoot(item: Node, roots) {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return (
    value &&
    roots.some(root => {
      const rootValue = getValue(root);
      return rootValue && rootValue.actor === value.actor;
    })
  );
}

function nodeIsBucket(item: Node): boolean {
  return getType(item) === NODE_TYPES.BUCKET;
}

function nodeIsEntries(item: Node): boolean {
  return getType(item) === NODE_TYPES.ENTRIES;
}

function nodeIsMapEntry(item: Node): boolean {
  return GripMapEntryRep.supportsObject(getValue(item));
}

function nodeHasChildren(item: Node): boolean {
  return Array.isArray(item.contents);
}

function nodeHasValue(item: Node): boolean {
  return item && item.contents && item.contents.hasOwnProperty("value");
}

function nodeHasGetterValue(item: Node): boolean {
  return item && item.contents && item.contents.hasOwnProperty("getterValue");
}

function nodeIsObject(item: Node): boolean {
  const value = getValue(item);
  return value && value.type === "object";
}

function nodeIsArrayLike(item: Node): boolean {
  const value = getValue(item);
  return GripArrayRep.supportsObject(value) || ArrayRep.supportsObject(value);
}

function nodeIsFunction(item: Node): boolean {
  const value = getValue(item);
  return value && value.class === "Function";
}

function nodeIsOptimizedOut(item: Node): boolean {
  const value = getValue(item);
  return !nodeHasChildren(item) && value && value.optimizedOut;
}

function nodeIsUninitializedBinding(item: Node): boolean {
  const value = getValue(item);
  return value && value.uninitialized;
}

// Used to check if an item represents a binding that exists in a sourcemap's
// original file content, but does not match up with a binding found in the
// generated code.
function nodeIsUnmappedBinding(item: Node): boolean {
  const value = getValue(item);
  return value && value.unmapped;
}

// Used to check if an item represents a binding that exists in the debugger's
// parser result, but does not match up with a binding returned by the
// debugger server.
function nodeIsUnscopedBinding(item: Node): boolean {
  const value = getValue(item);
  return value && value.unscoped;
}

function nodeIsMissingArguments(item: Node): boolean {
  const value = getValue(item);
  return !nodeHasChildren(item) && value && value.missingArguments;
}

function nodeHasProperties(item: Node): boolean {
  return !nodeHasChildren(item) && nodeIsObject(item);
}

function nodeIsPrimitive(item: Node): boolean {
  return (
    nodeIsBigInt(item) ||
    (!nodeHasChildren(item) &&
      !nodeHasProperties(item) &&
      !nodeIsEntries(item) &&
      !nodeIsMapEntry(item) &&
      !nodeHasAccessors(item) &&
      !nodeIsBucket(item) &&
      !nodeIsLongString(item))
  );
}

function nodeIsDefaultProperties(item: Node): boolean {
  return getType(item) === NODE_TYPES.DEFAULT_PROPERTIES;
}

function isDefaultWindowProperty(name: string): boolean {
  return WINDOW_PROPERTIES.includes(name);
}

function nodeIsPromise(item: Node): boolean {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class == "Promise";
}

function nodeIsProxy(item: Node): boolean {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class == "Proxy";
}

function nodeIsPrototype(item: Node): boolean {
  return getType(item) === NODE_TYPES.PROTOTYPE;
}

function nodeIsWindow(item: Node): boolean {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class == "Window";
}

function nodeIsGetter(item: Node): boolean {
  return getType(item) === NODE_TYPES.GET;
}

function nodeIsSetter(item: Node): boolean {
  return getType(item) === NODE_TYPES.SET;
}

function nodeIsBlock(item: Node) {
  return getType(item) === NODE_TYPES.BLOCK;
}

function nodeIsError(item: Node): boolean {
  return ErrorRep.supportsObject(getValue(item));
}

function nodeIsLongString(item: Node): boolean {
  return isLongString(getValue(item));
}

function nodeIsBigInt(item: Node): boolean {
  return BigIntRep.supportsObject(getValue(item));
}

function nodeHasFullText(item: Node): boolean {
  const value = getValue(item);
  return nodeIsLongString(item) && value.hasOwnProperty("fullText");
}

function nodeHasGetter(item: Node): boolean {
  const getter = getNodeGetter(item);
  return getter && getter.type !== "undefined";
}

function nodeHasSetter(item: Node): boolean {
  const setter = getNodeSetter(item);
  return setter && setter.type !== "undefined";
}

function nodeHasAccessors(item: Node): boolean {
  return nodeHasGetter(item) || nodeHasSetter(item);
}

function nodeSupportsNumericalBucketing(item: Node): boolean {
  // We exclude elements with entries since it's the <entries> node
  // itself that can have buckets.
  return (
    (nodeIsArrayLike(item) && !nodeHasEntries(item)) ||
    nodeIsEntries(item) ||
    nodeIsBucket(item)
  );
}

function nodeHasEntries(item: Node): boolean {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return (
    value.class === "Map" ||
    value.class === "Set" ||
    value.class === "WeakMap" ||
    value.class === "WeakSet" ||
    value.class === "Storage"
  );
}

function nodeHasAllEntriesInPreview(item: Node): boolean {
  const { preview } = getValue(item) || {};
  if (!preview) {
    return false;
  }

  const { entries, items, length, size } = preview;

  if (!entries && !items) {
    return false;
  }

  return entries ? entries.length === size : items.length === length;
}

function nodeNeedsNumericalBuckets(item: Node): boolean {
  return (
    nodeSupportsNumericalBucketing(item) &&
    getNumericalPropertiesCount(item) > MAX_NUMERICAL_PROPERTIES
  );
}

function makeNodesForPromiseProperties(item: Node): Array<Node> {
  const {
    promiseState: { reason, value, state },
  } = getValue(item);

  const properties = [];

  if (state) {
    properties.push(
      createNode({
        parent: item,
        name: "<state>",
        contents: { value: state },
        type: NODE_TYPES.PROMISE_STATE,
      })
    );
  }

  if (reason) {
    properties.push(
      createNode({
        parent: item,
        name: "<reason>",
        contents: { value: reason },
        type: NODE_TYPES.PROMISE_REASON,
      })
    );
  }

  if (value) {
    properties.push(
      createNode({
        parent: item,
        name: "<value>",
        contents: { value: value },
        type: NODE_TYPES.PROMISE_VALUE,
      })
    );
  }

  return properties;
}

function makeNodesForProxyProperties(
  loadedProps: GripProperties,
  item: Node
): Array<Node> {
  const { proxyHandler, proxyTarget } = loadedProps;

  return [
    createNode({
      parent: item,
      name: "<target>",
      contents: { value: proxyTarget },
      type: NODE_TYPES.PROXY_TARGET,
    }),
    createNode({
      parent: item,
      name: "<handler>",
      contents: { value: proxyHandler },
      type: NODE_TYPES.PROXY_HANDLER,
    }),
  ];
}

function makeNodesForEntries(item: Node): Node {
  const nodeName = "<entries>";
  const entriesPath = "<entries>";

  if (nodeHasAllEntriesInPreview(item)) {
    let entriesNodes = [];
    const { preview } = getValue(item);
    if (preview.entries) {
      entriesNodes = preview.entries.map(([key, value], index) => {
        return createNode({
          parent: item,
          name: index,
          path: createPath(entriesPath, index),
          contents: { value: GripMapEntryRep.createGripMapEntry(key, value) },
        });
      });
    } else if (preview.items) {
      entriesNodes = preview.items.map((value, index) => {
        return createNode({
          parent: item,
          name: index,
          path: createPath(entriesPath, index),
          contents: { value },
        });
      });
    }
    return createNode({
      parent: item,
      name: nodeName,
      contents: entriesNodes,
      type: NODE_TYPES.ENTRIES,
    });
  }
  return createNode({
    parent: item,
    name: nodeName,
    contents: null,
    type: NODE_TYPES.ENTRIES,
  });
}

function makeNodesForMapEntry(item: Node): Array<Node> {
  const nodeValue = getValue(item);
  if (!nodeValue || !nodeValue.preview) {
    return [];
  }

  const { key, value } = nodeValue.preview;

  return [
    createNode({
      parent: item,
      name: "<key>",
      contents: { value: key },
      type: NODE_TYPES.MAP_ENTRY_KEY,
    }),
    createNode({
      parent: item,
      name: "<value>",
      contents: { value },
      type: NODE_TYPES.MAP_ENTRY_VALUE,
    }),
  ];
}

function getNodeGetter(item: Node): ?Object {
  return item && item.contents ? item.contents.get : undefined;
}

function getNodeSetter(item: Node): ?Object {
  return item && item.contents ? item.contents.set : undefined;
}

function sortProperties(properties: Array<any>): Array<any> {
  return properties.sort((a, b) => {
    // Sort numbers in ascending order and sort strings lexicographically
    const aInt = parseInt(a, 10);
    const bInt = parseInt(b, 10);

    if (isNaN(aInt) || isNaN(bInt)) {
      return a > b ? 1 : -1;
    }

    return aInt - bInt;
  });
}

function makeNumericalBuckets(parent: Node): Array<Node> {
  const numProperties = getNumericalPropertiesCount(parent);

  // We want to have at most a hundred slices.
  const bucketSize =
    10 ** Math.max(2, Math.ceil(Math.log10(numProperties)) - 2);
  const numBuckets = Math.ceil(numProperties / bucketSize);

  const buckets = [];
  for (let i = 1; i <= numBuckets; i++) {
    const minKey = (i - 1) * bucketSize;
    const maxKey = Math.min(i * bucketSize - 1, numProperties - 1);
    const startIndex = nodeIsBucket(parent) ? parent.meta.startIndex : 0;
    const minIndex = startIndex + minKey;
    const maxIndex = startIndex + maxKey;
    const bucketName = `[${minIndex}…${maxIndex}]`;

    buckets.push(
      createNode({
        parent,
        name: bucketName,
        contents: null,
        type: NODE_TYPES.BUCKET,
        meta: {
          startIndex: minIndex,
          endIndex: maxIndex,
        },
      })
    );
  }
  return buckets;
}

function makeDefaultPropsBucket(
  propertiesNames: Array<string>,
  parent: Node,
  ownProperties: Object
): Array<Node> {
  const userPropertiesNames = [];
  const defaultProperties = [];

  propertiesNames.forEach(name => {
    if (isDefaultWindowProperty(name)) {
      defaultProperties.push(name);
    } else {
      userPropertiesNames.push(name);
    }
  });

  const nodes = makeNodesForOwnProps(
    userPropertiesNames,
    parent,
    ownProperties
  );

  if (defaultProperties.length > 0) {
    const defaultPropertiesNode = createNode({
      parent,
      name: "<default properties>",
      contents: null,
      type: NODE_TYPES.DEFAULT_PROPERTIES,
    });

    const defaultNodes = defaultProperties.map((name, index) =>
      createNode({
        parent: defaultPropertiesNode,
        name: maybeEscapePropertyName(name),
        path: createPath(index, name),
        contents: ownProperties[name],
      })
    );
    nodes.push(setNodeChildren(defaultPropertiesNode, defaultNodes));
  }
  return nodes;
}

function makeNodesForOwnProps(
  propertiesNames: Array<string>,
  parent: Node,
  ownProperties: Object
): Array<Node> {
  return propertiesNames.map(name =>
    createNode({
      parent,
      name: maybeEscapePropertyName(name),
      contents: ownProperties[name],
    })
  );
}

function makeNodesForProperties(
  objProps: GripProperties,
  parent: Node
): Array<Node> {
  const {
    ownProperties = {},
    ownSymbols,
    prototype,
    safeGetterValues,
  } = objProps;

  const parentValue = getValue(parent);

  const allProperties = { ...ownProperties, ...safeGetterValues };

  // Ignore properties that are neither non-concrete nor getters/setters.
  const propertiesNames = sortProperties(Object.keys(allProperties)).filter(
    name => {
      if (!allProperties[name]) {
        return false;
      }

      const properties = Object.getOwnPropertyNames(allProperties[name]);
      return properties.some(property =>
        ["value", "getterValue", "get", "set"].includes(property)
      );
    }
  );

  let nodes = [];
  if (parentValue && parentValue.class == "Window") {
    nodes = makeDefaultPropsBucket(propertiesNames, parent, allProperties);
  } else {
    nodes = makeNodesForOwnProps(propertiesNames, parent, allProperties);
  }

  if (Array.isArray(ownSymbols)) {
    ownSymbols.forEach((ownSymbol, index) => {
      nodes.push(
        createNode({
          parent,
          name: ownSymbol.name,
          path: `symbol-${index}`,
          contents: ownSymbol.descriptor || null,
        })
      );
    }, this);
  }

  if (nodeIsPromise(parent)) {
    nodes.push(...makeNodesForPromiseProperties(parent));
  }

  if (nodeHasEntries(parent)) {
    nodes.push(makeNodesForEntries(parent));
  }

  // Add accessor nodes if needed
  for (const name of propertiesNames) {
    const property = allProperties[name];
    if (property.get && property.get.type !== "undefined") {
      nodes.push(createGetterNode({ parent, property, name }));
    }

    if (property.set && property.set.type !== "undefined") {
      nodes.push(createSetterNode({ parent, property, name }));
    }
  }

  // Add the prototype if it exists and is not null
  if (prototype && prototype.type !== "null") {
    nodes.push(makeNodeForPrototype(objProps, parent));
  }

  return nodes;
}

function setNodeFullText(loadedProps: GripProperties, node: Node): Node {
  if (nodeHasFullText(node) || !nodeIsLongString(node)) {
    return node;
  }

  const { fullText } = loadedProps;
  if (nodeHasValue(node)) {
    node.contents.value.fullText = fullText;
  } else if (nodeHasGetterValue(node)) {
    node.contents.getterValue.fullText = fullText;
  }

  return node;
}

function makeNodeForPrototype(objProps: GripProperties, parent: Node): ?Node {
  const { prototype } = objProps || {};

  // Add the prototype if it exists and is not null
  if (prototype && prototype.type !== "null") {
    return createNode({
      parent,
      name: "<prototype>",
      contents: { value: prototype },
      type: NODE_TYPES.PROTOTYPE,
    });
  }

  return null;
}

function createNode(options: {
  parent: Node,
  name: string,
  contents: any,
  path?: string,
  type?: Symbol,
  meta?: Object,
}): ?Node {
  const {
    parent,
    name,
    path,
    contents,
    type = NODE_TYPES.GRIP,
    meta,
  } = options;

  if (contents === undefined) {
    return null;
  }

  // The path is important to uniquely identify the item in the entire
  // tree. This helps debugging & optimizes React's rendering of large
  // lists. The path will be separated by property name.

  return {
    parent,
    name,
    path: createPath(parent && parent.path, path || name),
    contents,
    type,
    meta,
  };
}

function createGetterNode({ parent, property, name }) {
  return createNode({
    parent,
    name: `<get ${name}()>`,
    contents: { value: property.get },
    type: NODE_TYPES.GET,
  });
}

function createSetterNode({ parent, property, name }) {
  return createNode({
    parent,
    name: `<set ${name}()>`,
    contents: { value: property.set },
    type: NODE_TYPES.SET,
  });
}

function setNodeChildren(node: Node, children: Array<Node>): Node {
  node.contents = children;
  return node;
}

function getEvaluatedItem(item: Node, evaluations: Evaluations): Node {
  if (!evaluations.has(item.path)) {
    return item;
  }

  return {
    ...item,
    contents: evaluations.get(item.path),
  };
}

function getChildrenWithEvaluations(options: {
  cachedNodes: CachedNodes,
  loadedProperties: LoadedProperties,
  item: Node,
  evaluations: Evaluations,
}): Array<Node> {
  const { item, loadedProperties, cachedNodes, evaluations } = options;

  const children = getChildren({
    loadedProperties,
    cachedNodes,
    item,
  });

  if (Array.isArray(children)) {
    return children.map(i => getEvaluatedItem(i, evaluations));
  }

  if (children) {
    return getEvaluatedItem(children, evaluations);
  }

  return [];
}

function getChildren(options: {
  cachedNodes: CachedNodes,
  loadedProperties: LoadedProperties,
  item: Node,
}): Array<Node> {
  const { cachedNodes, item, loadedProperties = new Map() } = options;

  const key = item.path;
  if (cachedNodes && cachedNodes.has(key)) {
    return cachedNodes.get(key);
  }

  const loadedProps = loadedProperties.get(key);
  const hasLoadedProps = loadedProperties.has(key);

  // Because we are dynamically creating the tree as the user
  // expands it (not precalculated tree structure), we cache child
  // arrays. This not only helps performance, but is necessary
  // because the expanded state depends on instances of nodes
  // being the same across renders. If we didn't do this, each
  // node would be a new instance every render.
  // If the node needs properties, we only add children to
  // the cache if the properties are loaded.
  const addToCache = (children: Array<Node>) => {
    if (cachedNodes) {
      cachedNodes.set(item.path, children);
    }
    return children;
  };

  // Nodes can either have children already, or be an object with
  // properties that we need to go and fetch.
  if (nodeHasChildren(item)) {
    return addToCache(item.contents);
  }

  if (nodeIsMapEntry(item)) {
    return addToCache(makeNodesForMapEntry(item));
  }

  if (nodeIsProxy(item) && hasLoadedProps) {
    return addToCache(makeNodesForProxyProperties(loadedProps, item));
  }

  if (nodeIsLongString(item) && hasLoadedProps) {
    // Set longString object's fullText to fetched one.
    return addToCache(setNodeFullText(loadedProps, item));
  }

  if (nodeNeedsNumericalBuckets(item) && hasLoadedProps) {
    // Even if we have numerical buckets, we should have loaded non indexed
    // properties.
    const bucketNodes = makeNumericalBuckets(item);
    return addToCache(
      bucketNodes.concat(makeNodesForProperties(loadedProps, item))
    );
  }

  if (!nodeIsEntries(item) && !nodeIsBucket(item) && !nodeHasProperties(item)) {
    return [];
  }

  if (!hasLoadedProps) {
    return [];
  }

  return addToCache(makeNodesForProperties(loadedProps, item));
}

function getParent(item: Node): Node | null {
  return item.parent;
}

function getNumericalPropertiesCount(item: Node): number {
  if (nodeIsBucket(item)) {
    return item.meta.endIndex - item.meta.startIndex + 1;
  }

  const value = getValue(getClosestGripNode(item));
  if (!value) {
    return 0;
  }

  if (GripArrayRep.supportsObject(value)) {
    return GripArrayRep.getLength(value);
  }

  if (GripMap.supportsObject(value)) {
    return GripMap.getLength(value);
  }

  // TODO: We can also have numerical properties on Objects, but at the
  // moment we don't have a way to distinguish them from non-indexed properties,
  // as they are all computed in a ownPropertiesLength property.

  return 0;
}

function getClosestGripNode(item: Node): Node | null {
  const type = getType(item);
  if (
    type !== NODE_TYPES.BUCKET &&
    type !== NODE_TYPES.DEFAULT_PROPERTIES &&
    type !== NODE_TYPES.ENTRIES
  ) {
    return item;
  }

  const parent = getParent(item);
  if (!parent) {
    return null;
  }

  return getClosestGripNode(parent);
}

function getClosestNonBucketNode(item: Node): Node | null {
  const type = getType(item);

  if (type !== NODE_TYPES.BUCKET) {
    return item;
  }

  const parent = getParent(item);
  if (!parent) {
    return null;
  }

  return getClosestNonBucketNode(parent);
}

function getParentGripNode(item: Node | null): Node | null {
  const parentNode = getParent(item);
  if (!parentNode) {
    return null;
  }

  return getClosestGripNode(parentNode);
}

function getParentGripValue(item: Node | null): any {
  const parentGripNode = getParentGripNode(item);
  if (!parentGripNode) {
    return null;
  }

  return getValue(parentGripNode);
}

function getNonPrototypeParentGripValue(item: Node | null): Node | null {
  const parentGripNode = getParentGripNode(item);
  if (!parentGripNode) {
    return null;
  }

  if (getType(parentGripNode) === NODE_TYPES.PROTOTYPE) {
    return getNonPrototypeParentGripValue(parentGripNode);
  }

  return getValue(parentGripNode);
}

function createPath(parentPath, path) {
  return parentPath ? `${parentPath}◦${path}` : path;
}

module.exports = {
  createNode,
  createGetterNode,
  createSetterNode,
  getActor,
  getChildren,
  getChildrenWithEvaluations,
  getClosestGripNode,
  getClosestNonBucketNode,
  getParent,
  getParentGripValue,
  getNonPrototypeParentGripValue,
  getNumericalPropertiesCount,
  getValue,
  makeNodesForEntries,
  makeNodesForPromiseProperties,
  makeNodesForProperties,
  makeNumericalBuckets,
  nodeHasAccessors,
  nodeHasAllEntriesInPreview,
  nodeHasChildren,
  nodeHasEntries,
  nodeHasProperties,
  nodeHasGetter,
  nodeHasSetter,
  nodeIsBlock,
  nodeIsBucket,
  nodeIsDefaultProperties,
  nodeIsEntries,
  nodeIsError,
  nodeIsLongString,
  nodeHasFullText,
  nodeIsFunction,
  nodeIsGetter,
  nodeIsMapEntry,
  nodeIsMissingArguments,
  nodeIsObject,
  nodeIsOptimizedOut,
  nodeIsPrimitive,
  nodeIsPromise,
  nodeIsPrototype,
  nodeIsProxy,
  nodeIsSetter,
  nodeIsUninitializedBinding,
  nodeIsUnmappedBinding,
  nodeIsUnscopedBinding,
  nodeIsWindow,
  nodeNeedsNumericalBuckets,
  nodeSupportsNumericalBucketing,
  setNodeChildren,
  sortProperties,
  NODE_TYPES,
};
