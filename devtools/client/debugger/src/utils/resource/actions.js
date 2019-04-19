/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  mutateState,
  createOrdered,
  createUnordered,
  type Resource,
  type Id,
  type Item,
  type Fields,
  type State,
  type Order,
  type UnorderedState,
  type OrderedState,
  type MutableResource,
  type MutableFields
} from "./core";

/**
 * Provide the default Redux state for an unordered store.
 */
export function createInitial<R: Resource>(
  fields: Fields<R>
): UnorderedState<R> {
  return createUnordered(fields);
}

/**
 * Provide the default Redux state for an ordered store.
 */
export function createInitialOrdered<R: Resource>(
  fields: Fields<R>
): OrderedState<R> {
  return createOrdered(fields);
}

/**
 * Splice a given item into the resource set at a given index.
 */
export function insertResourceAtIndex<R: Resource>(
  state: OrderedState<R>,
  resource: R,
  index: number
): OrderedState<R> {
  return mutateState(
    state,
    { resources: [resource], index },
    insertFieldsMutator,
    insertOrderMutator
  );
}
export function insertResourcesAtIndex<R: Resource>(
  state: OrderedState<R>,
  resources: Array<R>,
  index: number
): OrderedState<R> {
  if (resources.length === 0) {
    return state;
  }

  return mutateState(
    state,
    { resources, index },
    insertFieldsMutator,
    insertOrderMutator
  );
}

/**
 * Insert a given item into the resource set, at the end of the order if the
 * resource is an ordered one.
 */
export function insertResource<R: Resource, S: State<R>>(
  state: S,
  resource: R
): S {
  return mutateState(
    state,
    { resources: [resource] },
    insertFieldsMutator,
    insertOrderMutator
  );
}
export function insertResources<R: Resource, S: State<R>>(
  state: S,
  resources: Array<R>
): S {
  if (resources.length === 0) {
    return state;
  }

  return mutateState(
    state,
    { resources },
    insertFieldsMutator,
    insertOrderMutator
  );
}

/**
 * Remove a given item from the resource set, including any data.
 *
 * Note: This function here requires an Item because we want to encourage users
 * of this API to include the "item" type in their action payload. This allows
 * for secondary reducers that might be handling the event to have a bit more
 * metadata about an item, since they won't be able to look it up in the state.
 */
export function removeResource<R: Resource, S: State<R>>(
  state: S,
  item: Item<R>
): S {
  return mutateState(state, [item], removeFieldsMutator, removeOrderMutator);
}
export function removeResources<R: Resource, S: State<R>>(
  state: S,
  items: Array<Item<R>>
): S {
  if (items.length === 0) {
    return state;
  }
  return mutateState(state, items, removeFieldsMutator, removeOrderMutator);
}

/**
 * Set a single resource value for a single resource.
 */
export function setFieldValue<R: Resource, K: $Keys<R>, S: State<R>>(
  state: S,
  field: K,
  id: Id<R> | Item<R>,
  value: $ElementType<R, K>
): S {
  if (typeof id !== "string") {
    id = id.id;
  }

  return setFieldsValue(state, field, { [id]: value });
}

/**
 * Set a single resource value for multiple resources.
 */
export function setFieldValues<
  R: Resource,
  K: $Keys<$Rest<Fields<R>, { item: $ElementType<Fields<R>, "item"> }>>,
  S: State<R>
>(state: S, field: K, fieldValues: { [Id<R>]: $ElementType<R, K> }): S {
  return setFieldsValues(state, { [field]: fieldValues });
}

/**
 * Set multiple resource values for a single resource.
 */
export function setFieldsValue<R: Resource, S: State<R>>(
  state: S,
  id: Id<R> | Item<R>,
  idValues: MutableResource<R>
): S {
  if (typeof id !== "string") {
    id = id.id;
  }

  const fields = {};
  for (const key of Object.keys(idValues)) {
    fields[key] = { [id]: idValues[key] };
  }

  return setFieldsValues(state, fields);
}

/**
 * Set multiple resource values for a multiple resource.
 */
export function setFieldsValues<R: Resource, S: State<R>>(
  state: S,
  fields: $Shape<MutableFields<R>>
): S {
  return mutateState(state, fields, setFieldsMutator);
}

function setFieldsMutator<R: Resource>(
  newFields: $Shape<MutableFields<R>>,
  fields: Fields<R>
): void {
  if ("item" in newFields) {
    throw new Error(
      "Resource items cannot be updated. Remove the resource and recreate " +
        "it to change the item. Ideally though you should consider creating " +
        "an entirely new ID so that items are fully unique and immutable."
    );
  }

  for (const field of Object.keys(newFields)) {
    if (!Object.prototype.hasOwnProperty.call(fields, field)) {
      console.error(
        `Resource fields contains the property "${field}", which is unknown. ` +
          "The resource initial value should be updated to " +
          "include the new property. If you are seeing this and get no " +
          "Flow errors,  please make sure that you are not using any " +
          "optional properties in your primary resource item type and " +
          "that it is an exact object."
      );
    }

    const fieldsValues = newFields[field];
    if (fieldsValues) {
      fields[field] = { ...fields[field] };

      for (const id of Object.keys(fieldsValues)) {
        if (!Object.prototype.hasOwnProperty.call(fields.item, id)) {
          throw new Error(`Resource item ${id} not found, cannot set ${field}`);
        }

        fields[field][id] = fieldsValues[id];
      }
    }
  }
}

function insertFieldsMutator<R: Resource>(
  {
    resources
  }: {
    resources: Array<R>,
    index?: number
  },
  fields: Fields<R>
): void {
  for (const key of Object.keys(fields)) {
    fields[key] = { ...fields[key] };
  }

  for (const resource of resources) {
    const { id } = resource.item;
    if (Object.prototype.hasOwnProperty.call(fields.item, id)) {
      throw new Error(`Resource item ${id} already exists`);
    }

    for (const key of Object.keys(fields)) {
      fields[key][id] = resource[key];
    }

    for (const key of Object.keys(resource)) {
      if (!Object.prototype.hasOwnProperty.call(fields, key)) {
        console.error(
          `Resource ${id} contains the property "${key}", which is unknown.` +
            "The resource initial value should be updated to " +
            "include the new property. If you are seeing this and get no " +
            "Flow errors,  please make sure that you are not using any " +
            "optional properties in your primary resource item type and " +
            "that it is an exact object."
        );
      }
    }
  }
}
function insertOrderMutator<R: Resource>(
  {
    resources,
    index
  }: {
    resources: Array<R>,
    index?: number
  },
  order: Order<R>
): void {
  order.splice(
    typeof index === "number" ? index : order.length,
    0,
    ...resources.map(r => r.item.id)
  );
}

function removeFieldsMutator<R: Resource>(
  items: Array<Item<R>>,
  fields: Fields<R>
): void {
  for (const key of Object.keys(fields)) {
    fields[key] = { ...fields[key] };
  }

  for (const { id } of items) {
    if (!Object.prototype.hasOwnProperty.call(fields.item, id)) {
      throw new Error(`Resource item ${id} not found, cannot remove`);
    }

    for (const key of Object.keys(fields)) {
      delete fields[key][id];
    }
  }
}
function removeOrderMutator<R: Resource>(
  items: Array<Item<R>>,
  order: Order<R>
): Order<R> {
  const ids = new Set(items.map(item => item.id));
  return order.filter(id => ids.has(id));
}
