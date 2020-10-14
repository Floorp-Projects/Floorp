/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  enumEntries,
  enumIndexedProperties,
  enumNonIndexedProperties,
  getPrototype,
  enumSymbols,
  getFullText,
  getProxySlots,
} = require("./client");

const {
  getClosestGripNode,
  getClosestNonBucketNode,
  getFront,
  getValue,
  nodeHasAccessors,
  nodeHasProperties,
  nodeIsBucket,
  nodeIsDefaultProperties,
  nodeIsEntries,
  nodeIsMapEntry,
  nodeIsPrimitive,
  nodeIsProxy,
  nodeNeedsNumericalBuckets,
  nodeIsLongString,
} = require("./node");

import type {
  CreateLongStringFront,
  CreateObjectFront,
  GripProperties,
  LoadedProperties,
  Node,
} from "../types";

type Client = {
  createObjectFront?: CreateObjectFront,
  createLongStringFront?: CreateLongStringFront,
};

function loadItemProperties(
  item: Node,
  client: Client,
  loadedProperties: LoadedProperties
): Promise<GripProperties> {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);
  let front = getFront(gripItem);

  if (!front && value && client && client.getFrontByID) {
    front = client.getFrontByID(value.actor);
  }

  const getObjectFront = function() {
    if (!front) {
      front = client.createObjectFront(value);
    }

    return front;
  };

  const [start, end] = item.meta
    ? [item.meta.startIndex, item.meta.endIndex]
    : [];

  const promises = [];

  if (shouldLoadItemIndexedProperties(item, loadedProperties)) {
    promises.push(enumIndexedProperties(getObjectFront(), start, end));
  }

  if (shouldLoadItemNonIndexedProperties(item, loadedProperties)) {
    promises.push(enumNonIndexedProperties(getObjectFront(), start, end));
  }

  if (shouldLoadItemEntries(item, loadedProperties)) {
    promises.push(enumEntries(getObjectFront(), start, end));
  }

  if (shouldLoadItemPrototype(item, loadedProperties)) {
    promises.push(getPrototype(getObjectFront()));
  }

  if (shouldLoadItemSymbols(item, loadedProperties)) {
    promises.push(enumSymbols(getObjectFront(), start, end));
  }

  if (shouldLoadItemFullText(item, loadedProperties)) {
    const longStringFront = front || client.createLongStringFront(value);
    promises.push(getFullText(longStringFront, item));
  }

  if (shouldLoadItemProxySlots(item, loadedProperties)) {
    promises.push(getProxySlots(getObjectFront()));
  }

  return Promise.all(promises).then(mergeResponses);
}

function mergeResponses(responses: Array<Object>): Object {
  const data = {};

  for (const response of responses) {
    if (response.hasOwnProperty("ownProperties")) {
      data.ownProperties = { ...data.ownProperties, ...response.ownProperties };
    }

    if (response.ownSymbols && response.ownSymbols.length > 0) {
      data.ownSymbols = response.ownSymbols;
    }

    if (response.prototype) {
      data.prototype = response.prototype;
    }

    if (response.fullText) {
      data.fullText = response.fullText;
    }

    if (response.proxyTarget && response.proxyHandler) {
      data.proxyTarget = response.proxyTarget;
      data.proxyHandler = response.proxyHandler;
    }
  }

  return data;
}

function shouldLoadItemIndexedProperties(
  item: Node,
  loadedProperties: LoadedProperties = new Map()
): boolean {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return (
    value &&
    nodeHasProperties(gripItem) &&
    !loadedProperties.has(item.path) &&
    !nodeIsProxy(item) &&
    !nodeNeedsNumericalBuckets(item) &&
    !nodeIsEntries(getClosestNonBucketNode(item)) &&
    // The data is loaded when expanding the window node.
    !nodeIsDefaultProperties(item)
  );
}

function shouldLoadItemNonIndexedProperties(
  item: Node,
  loadedProperties: LoadedProperties = new Map()
): boolean {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return (
    value &&
    nodeHasProperties(gripItem) &&
    !loadedProperties.has(item.path) &&
    !nodeIsProxy(item) &&
    !nodeIsEntries(getClosestNonBucketNode(item)) &&
    !nodeIsBucket(item) &&
    // The data is loaded when expanding the window node.
    !nodeIsDefaultProperties(item)
  );
}

function shouldLoadItemEntries(
  item: Node,
  loadedProperties: LoadedProperties = new Map()
): boolean {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return (
    value &&
    nodeIsEntries(getClosestNonBucketNode(item)) &&
    !loadedProperties.has(item.path) &&
    !nodeNeedsNumericalBuckets(item)
  );
}

function shouldLoadItemPrototype(
  item: Node,
  loadedProperties: LoadedProperties = new Map()
): boolean {
  const value = getValue(item);

  return (
    value &&
    !loadedProperties.has(item.path) &&
    !nodeIsBucket(item) &&
    !nodeIsMapEntry(item) &&
    !nodeIsEntries(item) &&
    !nodeIsDefaultProperties(item) &&
    !nodeHasAccessors(item) &&
    !nodeIsPrimitive(item) &&
    !nodeIsLongString(item) &&
    !nodeIsProxy(item)
  );
}

function shouldLoadItemSymbols(
  item: Node,
  loadedProperties: LoadedProperties = new Map()
): boolean {
  const value = getValue(item);

  return (
    value &&
    !loadedProperties.has(item.path) &&
    !nodeIsBucket(item) &&
    !nodeIsMapEntry(item) &&
    !nodeIsEntries(item) &&
    !nodeIsDefaultProperties(item) &&
    !nodeHasAccessors(item) &&
    !nodeIsPrimitive(item) &&
    !nodeIsLongString(item) &&
    !nodeIsProxy(item)
  );
}

function shouldLoadItemFullText(
  item: Node,
  loadedProperties: LoadedProperties = new Map()
) {
  return !loadedProperties.has(item.path) && nodeIsLongString(item);
}

function shouldLoadItemProxySlots(
  item: Node,
  loadedProperties: LoadedProperties = new Map()
): boolean {
  return !loadedProperties.has(item.path) && nodeIsProxy(item);
}

module.exports = {
  loadItemProperties,
  mergeResponses,
  shouldLoadItemEntries,
  shouldLoadItemIndexedProperties,
  shouldLoadItemNonIndexedProperties,
  shouldLoadItemPrototype,
  shouldLoadItemSymbols,
  shouldLoadItemFullText,
  shouldLoadItemProxySlots,
};
