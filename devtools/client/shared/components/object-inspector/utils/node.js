/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  maybeEscapePropertyName,
} = require("devtools/client/shared/components/reps/reps/rep-utils");
const ArrayRep = require("devtools/client/shared/components/reps/reps/array");
const GripArrayRep = require("devtools/client/shared/components/reps/reps/grip-array");
const GripMap = require("devtools/client/shared/components/reps/reps/grip-map");
const GripEntryRep = require("devtools/client/shared/components/reps/reps/grip-entry");
const ErrorRep = require("devtools/client/shared/components/reps/reps/error");
const BigIntRep = require("devtools/client/shared/components/reps/reps/big-int");
const {
  isLongString,
} = require("devtools/client/shared/components/reps/reps/string");

const MAX_NUMERICAL_PROPERTIES = 100;

const NODE_TYPES = {
  BUCKET: Symbol("[n…m]"),
  DEFAULT_PROPERTIES: Symbol("<default properties>"),
  ENTRIES: Symbol("<entries>"),
  GET: Symbol("<get>"),
  GRIP: Symbol("GRIP"),
  JSONML: Symbol("JsonML"),
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

let WINDOW_PROPERTIES = {};

if (typeof window === "object") {
  WINDOW_PROPERTIES = Object.getOwnPropertyNames(window);
}

function getType(item) {
  return item.type;
}

function getValue(item) {
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

function getFront(item) {
  return item && item.contents && item.contents.front;
}

function getActor(item, roots) {
  const isRoot = isNodeRoot(item, roots);
  const value = getValue(item);
  return isRoot || !value ? null : value.actor;
}

function isNodeRoot(item, roots) {
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

function nodeIsBucket(item) {
  return getType(item) === NODE_TYPES.BUCKET;
}

function nodeIsEntries(item) {
  return getType(item) === NODE_TYPES.ENTRIES;
}

function nodeIsMapEntry(item) {
  return GripEntryRep.supportsObject(getValue(item));
}

function nodeHasChildren(item) {
  return Array.isArray(item.contents);
}

function nodeHasCustomFormatter(item) {
  return item?.contents?.value?.useCustomFormatter === true && Array.isArray(item?.contents?.value?.header);
}

function nodeHasCustomFormattedBody(item) {
  return item?.contents?.value?.hasBody === true;
}

function nodeHasValue(item) {
  return item && item.contents && item.contents.hasOwnProperty("value");
}

function nodeHasGetterValue(item) {
  return item && item.contents && item.contents.hasOwnProperty("getterValue");
}

function nodeIsObject(item) {
  const value = getValue(item);
  return value && value.type === "object";
}

function nodeIsArrayLike(item) {
  const value = getValue(item);
  return GripArrayRep.supportsObject(value) || ArrayRep.supportsObject(value);
}

function nodeIsFunction(item) {
  const value = getValue(item);
  return value && value.class === "Function";
}

function nodeIsOptimizedOut(item) {
  const value = getValue(item);
  return !nodeHasChildren(item) && value && value.optimizedOut;
}

function nodeIsUninitializedBinding(item) {
  const value = getValue(item);
  return value && value.uninitialized;
}

// Used to check if an item represents a binding that exists in a sourcemap's
// original file content, but does not match up with a binding found in the
// generated code.
function nodeIsUnmappedBinding(item) {
  const value = getValue(item);
  return value && value.unmapped;
}

// Used to check if an item represents a binding that exists in the debugger's
// parser result, but does not match up with a binding returned by the
// devtools server.
function nodeIsUnscopedBinding(item) {
  const value = getValue(item);
  return value && value.unscoped;
}

function nodeIsMissingArguments(item) {
  const value = getValue(item);
  return !nodeHasChildren(item) && value && value.missingArguments;
}

function nodeHasProperties(item) {
  return !nodeHasChildren(item) && nodeIsObject(item);
}

function nodeIsPrimitive(item) {
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

function nodeIsDefaultProperties(item) {
  return getType(item) === NODE_TYPES.DEFAULT_PROPERTIES;
}

function isDefaultWindowProperty(name) {
  return WINDOW_PROPERTIES.includes(name);
}

function nodeIsPromise(item) {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class == "Promise";
}

function nodeIsProxy(item) {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class == "Proxy";
}

function nodeIsPrototype(item) {
  return getType(item) === NODE_TYPES.PROTOTYPE;
}

function nodeIsWindow(item) {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class == "Window";
}

function nodeIsGetter(item) {
  return getType(item) === NODE_TYPES.GET;
}

function nodeIsSetter(item) {
  return getType(item) === NODE_TYPES.SET;
}

function nodeIsBlock(item) {
  return getType(item) === NODE_TYPES.BLOCK;
}

function nodeIsError(item) {
  return ErrorRep.supportsObject(getValue(item));
}

function nodeIsLongString(item) {
  return isLongString(getValue(item));
}

function nodeIsBigInt(item) {
  return BigIntRep.supportsObject(getValue(item));
}

function nodeHasFullText(item) {
  const value = getValue(item);
  return nodeIsLongString(item) && value.hasOwnProperty("fullText");
}

function nodeHasGetter(item) {
  const getter = getNodeGetter(item);
  return getter && getter.type !== "undefined";
}

function nodeHasSetter(item) {
  const setter = getNodeSetter(item);
  return setter && setter.type !== "undefined";
}

function nodeHasAccessors(item) {
  return nodeHasGetter(item) || nodeHasSetter(item);
}

function nodeSupportsNumericalBucketing(item) {
  // We exclude elements with entries since it's the <entries> node
  // itself that can have buckets.
  return (
    (nodeIsArrayLike(item) && !nodeHasEntries(item)) ||
    nodeIsEntries(item) ||
    nodeIsBucket(item)
  );
}

function nodeHasEntries(item) {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  const className = value.class;
  return (
    className === "Map" ||
    className === "Set" ||
    className === "WeakMap" ||
    className === "WeakSet" ||
    className === "Storage" ||
    className === "URLSearchParams" ||
    // @backward-compat { version 105 } Support for enumerate Headers entries was
    // added in 105. When connecting to older server, we don't want to show the <entries>
    // node for them. The extra check can be removed once 105 hits release.
    (className === "Headers" && Array.isArray(value.preview?.entries)) ||
    // @backward-compat { version 106 } Support for enumerate FormData entries was
    // added in 105. When connecting to older server, we don't want to show the <entries>
    // node for them. The extra check can be removed once 105 hits release.
    (className === "FormData" && Array.isArray(value.preview?.entries))
  );
}

function nodeNeedsNumericalBuckets(item) {
  return (
    nodeSupportsNumericalBucketing(item) &&
    getNumericalPropertiesCount(item) > MAX_NUMERICAL_PROPERTIES
  );
}

function makeNodesForPromiseProperties(loadedProps, item) {
  const { reason, value, state } = loadedProps.promiseState;
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
        contents: {
          value: reason.getGrip ? reason.getGrip() : reason,
          front: reason.getGrip ? reason : null,
        },
        type: NODE_TYPES.PROMISE_REASON,
      })
    );
  }

  if (value) {
    properties.push(
      createNode({
        parent: item,
        name: "<value>",
        contents: {
          value: value.getGrip ? value.getGrip() : value,
          front: value.getGrip ? value : null,
        },
        type: NODE_TYPES.PROMISE_VALUE,
      })
    );
  }

  return properties;
}

function makeNodesForProxyProperties(loadedProps, item) {
  const { proxyHandler, proxyTarget } = loadedProps;

  const isProxyHandlerFront = proxyHandler && proxyHandler.getGrip;
  const proxyHandlerGrip = isProxyHandlerFront
    ? proxyHandler.getGrip()
    : proxyHandler;
  const proxyHandlerFront = isProxyHandlerFront ? proxyHandler : null;

  const isProxyTargetFront = proxyTarget && proxyTarget.getGrip;
  const proxyTargetGrip = isProxyTargetFront
    ? proxyTarget.getGrip()
    : proxyTarget;
  const proxyTargetFront = isProxyTargetFront ? proxyTarget : null;

  return [
    createNode({
      parent: item,
      name: "<target>",
      contents: { value: proxyTargetGrip, front: proxyTargetFront },
      type: NODE_TYPES.PROXY_TARGET,
    }),
    createNode({
      parent: item,
      name: "<handler>",
      contents: { value: proxyHandlerGrip, front: proxyHandlerFront },
      type: NODE_TYPES.PROXY_HANDLER,
    }),
  ];
}

function makeJsonMlNode(loadedProps, item) {
  return [
    createNode({
      parent: item,
      path: "body",
      contents: {
        value: {
          header: loadedProps.customFormatterBody,
          useCustomFormatter: true,
        },
      },
      type: NODE_TYPES.JSONML,
    }),
  ];
}

function makeNodesForEntries(item) {
  const nodeName = "<entries>";

  return createNode({
    parent: item,
    name: nodeName,
    contents: null,
    type: NODE_TYPES.ENTRIES,
  });
}

function makeNodesForMapEntry(item) {
  const nodeValue = getValue(item);
  if (!nodeValue || !nodeValue.preview) {
    return [];
  }

  const { key, value } = nodeValue.preview;
  const isKeyFront = key && key.getGrip;
  const keyGrip = isKeyFront ? key.getGrip() : key;
  const keyFront = isKeyFront ? key : null;

  const isValueFront = value && value.getGrip;
  const valueGrip = isValueFront ? value.getGrip() : value;
  const valueFront = isValueFront ? value : null;

  return [
    createNode({
      parent: item,
      name: "<key>",
      contents: { value: keyGrip, front: keyFront },
      type: NODE_TYPES.MAP_ENTRY_KEY,
    }),
    createNode({
      parent: item,
      name: "<value>",
      contents: { value: valueGrip, front: valueFront },
      type: NODE_TYPES.MAP_ENTRY_VALUE,
    }),
  ];
}

function getNodeGetter(item) {
  return item && item.contents ? item.contents.get : undefined;
}

function getNodeSetter(item) {
  return item && item.contents ? item.contents.set : undefined;
}

function sortProperties(properties) {
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

function makeNumericalBuckets(parent) {
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

function makeDefaultPropsBucket(propertiesNames, parent, ownProperties) {
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

    const defaultNodes = makeNodesForOwnProps(
      defaultProperties,
      defaultPropertiesNode,
      ownProperties
    );
    nodes.push(setNodeChildren(defaultPropertiesNode, defaultNodes));
  }
  return nodes;
}

function makeNodesForOwnProps(propertiesNames, parent, ownProperties) {
  return propertiesNames.map(name => {
    const property = ownProperties[name];

    let propertyValue = property;
    if (property && property.hasOwnProperty("getterValue")) {
      propertyValue = property.getterValue;
    } else if (property && property.hasOwnProperty("value")) {
      propertyValue = property.value;
    }

    // propertyValue can be a front (LongString or Object) or a primitive grip.
    const isFront = propertyValue && propertyValue.getGrip;
    const front = isFront ? propertyValue : null;
    const grip = isFront ? front.getGrip() : propertyValue;

    return createNode({
      parent,
      name: maybeEscapePropertyName(name),
      propertyName: name,
      contents: {
        ...(property || {}),
        value: grip,
        front,
      },
    });
  });
}

function makeNodesForProperties(objProps, parent) {
  const {
    ownProperties = {},
    ownSymbols,
    privateProperties,
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

  const isParentNodeWindow = parentValue && parentValue.class == "Window";
  const nodes = isParentNodeWindow
    ? makeDefaultPropsBucket(propertiesNames, parent, allProperties)
    : makeNodesForOwnProps(propertiesNames, parent, allProperties);

  if (Array.isArray(ownSymbols)) {
    ownSymbols.forEach((ownSymbol, index) => {
      const descriptorValue = ownSymbol?.descriptor?.value;
      const hasGrip = descriptorValue?.getGrip;
      const symbolGrip = hasGrip ? descriptorValue.getGrip() : descriptorValue;
      const symbolFront = hasGrip ? ownSymbol.descriptor.value : null;

      nodes.push(
        createNode({
          parent,
          name: ownSymbol.name,
          path: `symbol-${index}`,
          contents: {
            value: symbolGrip,
            front: symbolFront,
          },
        })
      );
    }, this);
  }

  if (Array.isArray(privateProperties)) {
    privateProperties.forEach((privateProperty, index) => {
      const descriptorValue = privateProperty?.descriptor?.value;
      const hasGrip = descriptorValue?.getGrip;
      const privatePropertyGrip = hasGrip
        ? descriptorValue.getGrip()
        : descriptorValue;
      const privatePropertyFront = hasGrip
        ? privateProperty.descriptor.value
        : null;

      nodes.push(
        createNode({
          parent,
          name: privateProperty.name,
          path: `private-${index}`,
          contents: {
            value: privatePropertyGrip,
            front: privatePropertyFront,
          },
        })
      );
    }, this);
  }

  if (nodeIsPromise(parent)) {
    nodes.push(...makeNodesForPromiseProperties(objProps, parent));
  }

  if (nodeHasEntries(parent)) {
    nodes.push(makeNodesForEntries(parent));
  }

  // Add accessor nodes if needed
  const defaultPropertiesNode = isParentNodeWindow
    ? nodes.find(node => nodeIsDefaultProperties(node))
    : null;

  for (const name of propertiesNames) {
    const property = allProperties[name];
    const isDefaultProperty =
      isParentNodeWindow &&
      defaultPropertiesNode &&
      isDefaultWindowProperty(name);
    const parentNode = isDefaultProperty ? defaultPropertiesNode : parent;
    const parentContentsArray =
      isDefaultProperty && defaultPropertiesNode
        ? defaultPropertiesNode.contents
        : nodes;

    if (property.get && property.get.type !== "undefined") {
      parentContentsArray.push(
        createGetterNode({
          parent: parentNode,
          property,
          name,
        })
      );
    }

    if (property.set && property.set.type !== "undefined") {
      parentContentsArray.push(
        createSetterNode({
          parent: parentNode,
          property,
          name,
        })
      );
    }
  }

  // Add the prototype if it exists and is not null
  if (prototype && prototype.type !== "null") {
    nodes.push(makeNodeForPrototype(objProps, parent));
  }

  return nodes;
}

function setNodeFullText(loadedProps, node) {
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

function makeNodeForPrototype(objProps, parent) {
  const { prototype } = objProps || {};

  // Add the prototype if it exists and is not null
  if (prototype && prototype.type !== "null") {
    return createNode({
      parent,
      name: "<prototype>",
      contents: {
        value: prototype.getGrip ? prototype.getGrip() : prototype,
        front: prototype.getGrip ? prototype : null,
      },
      type: NODE_TYPES.PROTOTYPE,
    });
  }

  return null;
}

function createNode(options) {
  const {
    parent,
    name,
    propertyName,
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
    // `name` can be escaped; propertyName contains the original property name.
    propertyName,
    path: createPath(parent && parent.path, path || name),
    contents,
    type,
    meta,
  };
}

function createGetterNode({ parent, property, name }) {
  const isFront = property.get && property.get.getGrip;
  const grip = isFront ? property.get.getGrip() : property.get;
  const front = isFront ? property.get : null;

  return createNode({
    parent,
    name: `<get ${name}()>`,
    contents: { value: grip, front },
    type: NODE_TYPES.GET,
  });
}

function createSetterNode({ parent, property, name }) {
  const isFront = property.set && property.set.getGrip;
  const grip = isFront ? property.set.getGrip() : property.set;
  const front = isFront ? property.set : null;

  return createNode({
    parent,
    name: `<set ${name}()>`,
    contents: { value: grip, front },
    type: NODE_TYPES.SET,
  });
}

function setNodeChildren(node, children) {
  node.contents = children;
  return node;
}

function getEvaluatedItem(item, evaluations) {
  if (!evaluations.has(item.path)) {
    return item;
  }

  const evaluation = evaluations.get(item.path);
  const isFront =
    evaluation && evaluation.getterValue && evaluation.getterValue.getGrip;

  const contents = isFront
    ? {
        getterValue: evaluation.getterValue.getGrip(),
        front: evaluation.getterValue,
      }
    : evaluations.get(item.path);

  return {
    ...item,
    contents,
  };
}

function getChildrenWithEvaluations(options) {
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

function getChildren(options) {
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
  const addToCache = children => {
    if (cachedNodes) {
      cachedNodes.set(item.path, children);
    }
    return children;
  };

  if (nodeHasCustomFormattedBody(item) && hasLoadedProps) {
    return addToCache(makeJsonMlNode(loadedProps, item));
  }

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

// Builds an expression that resolves to the value of the item in question
// e.g. `b` in { a: { b: 2 } } resolves to `a.b`
function getPathExpression(item) {
  if (item && item.parent) {
    const parent = nodeIsBucket(item.parent) ? item.parent.parent : item.parent;
    return `${getPathExpression(parent)}.${item.name}`;
  }

  return item.name;
}

function getParent(item) {
  return item.parent;
}

function getNumericalPropertiesCount(item) {
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

function getClosestGripNode(item) {
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

function getClosestNonBucketNode(item) {
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

function getParentGripNode(item) {
  const parentNode = getParent(item);
  if (!parentNode) {
    return null;
  }

  return getClosestGripNode(parentNode);
}

function getParentGripValue(item) {
  const parentGripNode = getParentGripNode(item);
  if (!parentGripNode) {
    return null;
  }

  return getValue(parentGripNode);
}

function getParentFront(item) {
  const parentGripNode = getParentGripNode(item);
  if (!parentGripNode) {
    return null;
  }

  return getFront(parentGripNode);
}

function getNonPrototypeParentGripValue(item) {
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
  getEvaluatedItem,
  getFront,
  getPathExpression,
  getParent,
  getParentFront,
  getParentGripValue,
  getNonPrototypeParentGripValue,
  getNumericalPropertiesCount,
  getValue,
  makeJsonMlNode,
  makeNodesForEntries,
  makeNodesForPromiseProperties,
  makeNodesForProperties,
  makeNumericalBuckets,
  nodeHasAccessors,
  nodeHasChildren,
  nodeHasCustomFormattedBody,
  nodeHasCustomFormatter,
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
