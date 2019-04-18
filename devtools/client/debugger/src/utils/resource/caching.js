/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getFields, type Resource, type Id, type State } from "./core";

/**
 * A simple wrapper on createFieldReducer to take a set of IDs and get the
 * current values for a given field, in the target resource.
 *
 * Caches results based on object identity. See createFieldReducer for
 * more information about what specific caching is performed.
 */
export type FieldByIDGetter<R: Resource, K: $Keys<R>> = FieldByIDMapper<
  R,
  $ElementType<R, K>
>;
export function createFieldByIDGetter<R: Resource, K: $Keys<R>>(
  field: K
): FieldByIDGetter<R, K> {
  return createFieldByIDMapper(field, v => v);
}

/**
 * A simple wrapper on createFieldReducer for mapping a set of IDs to
 * a set of values.
 *
 * Caches results based on object identity. See createFieldReducer for
 * more information about what specific caching is performed.
 */
export type FieldByIDMapper<R: Resource, T> = FieldByIDReducer<R, Array<T>>;
export function createFieldByIDMapper<R: Resource, K: $Keys<R>, T>(
  field: K,
  mapper: (value: $ElementType<R, K>, id: Id<R>, i: number) => T
): FieldByIDMapper<R, T> {
  return createFieldByIDReducer(
    field,
    (acc, value, id, i) => {
      acc.push(mapper(value, id, i));
      return acc;
    },
    () => []
  );
}

/**
 * Generic utility for taking a given array of IDs for a type, and loading
 * data for those IDs from a given field for that type, and then reducing
 * those values into a final result.
 *
 * If the returned callback is passed the exact same array of IDs and the
 * same state value as the previous call with those IDs, it will return
 * the exact same result as last time.
 *
 * If the returned callback is passed the exact same array of IDs and a
 * different state, we will check if all of the same IDs point an unchanged
 * values, we will also reused the previously calculated result.
 *
 * If the set of values referenced by the IDs has changed, then the reducer
 * will be executed to create a new cached value for this set of IDs.
 */
export type FieldByIDReducer<R: Resource, T> = (
  State<R>,
  $ReadOnlyArray<Id<R>> | Set<Id<R>>
) => T;
export function createFieldByIDReducer<R: Resource, K: $Keys<R>, T>(
  field: K,
  reducer: (acc: T, value: $ElementType<R, K>, id: Id<R>, i: number) => T,
  initial: () => T
): FieldByIDReducer<R, T> {
  const cache = new WeakMap();
  const handler = (acc, { id, value }, i) => reducer(acc, value, id, i);

  return (state, ids) => {
    const fields = getFields(state);
    const fieldValues = fields[field];

    if (!fieldValues) {
      throw new Error(`Field "${field}" does not exist in this resource.`);
    }

    let result = cache.get(ids);
    if (!result || result.fieldValues !== fieldValues) {
      if (!Array.isArray(ids)) {
        ids = Array.from(ids);
      }

      const items = ids.map(id => {
        if (!Object.prototype.hasOwnProperty.call(fields.item, id)) {
          throw new Error(`Resource item ${id} not found`);
        }

        return {
          id,
          value: fieldValues[id]
        };
      });

      // If the resource object itself changed but we still have the same set
      // of result values, we can also reuse the previously calculated result.
      if (
        !result ||
        result.items.length !== items.length ||
        result.items.some(
          (item, i) => items[i].id !== item.id || items[i].value !== item.value
        )
      ) {
        result = {
          fieldValues,
          items,
          value: items.reduce(handler, initial())
        };
        cache.set(ids, result);
      } else {
        result.fieldValues = fieldValues;
      }
    }

    return result.value;
  };
}

/**
 * Builds a memozed result based on all of the values for a given field.
 */
export type FieldReducer<R: Resource, T> = (State<R>) => T;
export function createFieldReducer<R: Resource, K: $Keys<R>, T>(
  field: K,
  reducer: (acc: T, value: $ElementType<R, K>, id: Id<R>) => T,
  initial: () => T
): FieldReducer<R, T> {
  const cache = new WeakMap();

  return state => {
    const fieldValues = getFields(state)[field];

    if (cache.has(fieldValues)) {
      // Flow isn't quite smary enough to know that "has" returning true means
      // that we'll always get the expected value, so we have to cast to 'any'.
      // We specifically use 'has' because we want be flexible and allow the
      // 'reducer' function to return 'undefined' if it wants to.
      return (cache.get(fieldValues): any);
    }

    let result = initial();
    for (const id of Object.keys(fieldValues)) {
      result = reducer(result, fieldValues[id], id);
    }

    cache.set(fieldValues, result);
    return result;
  };
}
