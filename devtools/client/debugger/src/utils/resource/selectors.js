/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getOrder,
  getFields,
  type Resource,
  type Id,
  type Item,
  type OrderedState,
  type State
} from "./core";

/**
 * Verify that a resource exists with the given ID.
 */
export function hasResource<R: Resource>(
  state: State<R>,
  id: Id<R> | Item<R>
): boolean {
  if (typeof id !== "string") {
    id = id.id;
  }

  return Object.prototype.hasOwnProperty.call(getFields(state).item, id);
}

/**
 * Get the overall Resource value for a given ID.
 *
 * When using this function, please consider whether it may make more sense
 * to create a cached selector using one of the 'createFieldXXXXXXX' helpers.
 */
export function getResource<R: Resource>(
  state: State<R>,
  id: Id<R> | Item<R>
): R {
  if (typeof id !== "string") {
    id = id.id;
  }

  if (!hasResource(state, id)) {
    throw new Error(`Resource ${id} not found`);
  }
  const fields = getFields(state);

  // We have to do some typecasting here because while we know that the the
  // types are the same, Flow isn't quite able to guarantee that the returned
  // Resource here will actually match the type.
  const resource: { [string]: mixed } = {};
  for (const key of Object.keys(fields)) {
    resource[key] = fields[key][id];
  }
  return (resource: any);
}

/**
 * Get a list of all of the items in an ordered resource.
 */
const listCache: WeakMap<Array<mixed>, Array<mixed>> = new WeakMap();
export function listItems<R: Resource>(state: OrderedState<R>): Array<Item<R>> {
  const order = getOrder(state);

  // Note: The 'any' casts here are necessary because Flow has no way to know
  // that the 'R' type of 'order' and 'items' is required to match.
  let items: Array<Item<R>> | void = (listCache.get((order: any)): any);
  if (!items) {
    // Since items can't be changed, the order array's identity can fully
    // cache this item load. Changes to items would have changed the order too.
    items = order.map(id => getItem(state, id));
    listCache.set((order: any), (items: any));
  }
  return items;
}

export function getItem<R: Resource>(state: State<R>, id: Id<R>): Item<R> {
  if (!hasResource(state, id)) {
    throw new Error(`Resource item ${id} not found`);
  }

  return getFields(state).item[id];
}

/**
 * Look up a given item in the resource based on its ID. Throws if the item
 * is not found.
 */
export function getFieldValue<R: Resource, K: $Keys<R>>(
  state: State<R>,
  field: K,
  id: Id<R> | Item<R>
): $ElementType<R, K> {
  if (typeof id !== "string") {
    id = id.id;
  }

  if (!hasResource(state, id)) {
    throw new Error(`Resource item ${id} not found`);
  }
  const fieldValues = getFields(state)[field];
  if (!fieldValues) {
    throw new Error(`Resource corrupt: Field "${field}" not found`);
  }

  // We don't bother to check if this ID exists in 'values' because if it
  // does not, and things typechecked, then it means that the inserted
  // resource was missing that key and returning undefined is fine.
  return fieldValues[id];
}
