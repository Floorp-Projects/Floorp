/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import type {
  GripProperties,
  ObjectFront,
  PropertiesIterator,
  Node,
  LongStringFront,
} from "../types";

const { getValue, nodeHasFullText } = require("../utils/node");

async function enumIndexedProperties(
  objectFront: ObjectFront,
  start: ?number,
  end: ?number
): Promise<{ ownProperties?: Object }> {
  try {
    const iterator = await objectFront.enumProperties({
      ignoreNonIndexedProperties: true,
    });
    const response = await iteratorSlice(iterator, start, end);
    return response;
  } catch (e) {
    console.error("Error in enumIndexedProperties", e);
    return {};
  }
}

async function enumNonIndexedProperties(
  objectFront: ObjectFront,
  start: ?number,
  end: ?number
): Promise<{ ownProperties?: Object }> {
  try {
    const iterator = await objectFront.enumProperties({
      ignoreIndexedProperties: true,
    });
    const response = await iteratorSlice(iterator, start, end);
    return response;
  } catch (e) {
    console.error("Error in enumNonIndexedProperties", e);
    return {};
  }
}

async function enumEntries(
  objectFront: ObjectFront,
  start: ?number,
  end: ?number
): Promise<{ ownProperties?: Object }> {
  try {
    const iterator = await objectFront.enumEntries();
    const response = await iteratorSlice(iterator, start, end);
    return response;
  } catch (e) {
    console.error("Error in enumEntries", e);
    return {};
  }
}

async function enumSymbols(
  objectFront: ObjectFront,
  start: ?number,
  end: ?number
): Promise<{ ownSymbols?: Array<Object> }> {
  try {
    const iterator = await objectFront.enumSymbols();
    const response = await iteratorSlice(iterator, start, end);
    return response;
  } catch (e) {
    console.error("Error in enumSymbols", e);
    return {};
  }
}

async function getPrototype(
  objectFront: ObjectFront
): ?Promise<{ prototype?: Object }> {
  if (typeof objectFront.getPrototype !== "function") {
    console.error("objectFront.getPrototype is not a function");
    return Promise.resolve({});
  }
  return objectFront.getPrototype();
}

async function getFullText(
  longStringFront: LongStringFront,
  item: Node
): Promise<{ fullText?: string }> {
  const { initial, fullText, length } = getValue(item);
  // Return fullText property if it exists so that it can be added to the
  // loadedProperties map.
  if (nodeHasFullText(item)) {
    return { fullText };
  }

  try {
    const substring = await longStringFront.substring(initial.length, length);
    return {
      fullText: initial + substring,
    };
  } catch (e) {
    console.error("LongStringFront.substring", e);
    throw e;
  }
}

async function getProxySlots(
  objectFront: ObjectFront
): Promise<{ proxyTarget?: Object, proxyHandler?: Object }> {
  return objectFront.getProxySlots();
}

function iteratorSlice(
  iterator: PropertiesIterator,
  start: ?number,
  end: ?number
): Promise<GripProperties> {
  start = start || 0;
  const count = end ? end - start + 1 : iterator.count;

  if (count === 0) {
    return Promise.resolve({});
  }
  return iterator.slice(start, count);
}

module.exports = {
  enumEntries,
  enumIndexedProperties,
  enumNonIndexedProperties,
  enumSymbols,
  getPrototype,
  getFullText,
  getProxySlots,
};
